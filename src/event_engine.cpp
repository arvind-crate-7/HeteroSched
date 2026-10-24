#include "heterosched/event_engine.hpp"
#include <chrono>
#include <iostream>
#include <cassert>

namespace heterosched {

EventEngine::EventEngine(TaskGraph graph, SimulatorConfig config, std::unique_ptr<SchedulingPolicy> policy)
    : graph_(std::move(graph)),
      config_(config),
      resources_(config.cpu_slots, config.gpu_slots, config.gpu_vram_bytes),
      transfer_engine_(config.dma_engines, config.pcie_bandwidth_bytes_per_tick, config.pcie_setup_latency_ticks),
      policy_(std::move(policy)) {}

void EventEngine::schedule_event(SimTick tick, EventPhase phase, EventType type, TaskId task_id,
                                DeviceId dev, SlotId slot, DmaEngineId dma, ByteCount bytes, std::string meta) {
    event_queue_.push(Event{
        .tick = tick,
        .phase = phase,
        .sequence_number = sequence_counter_++,
        .type = type,
        .task_id = task_id,
        .device_id = dev,
        .slot_id = slot,
        .dma_id = dma,
        .bytes = bytes,
        .metadata = std::move(meta)
    });
}

void EventEngine::run() {
    auto wall_start = std::chrono::steady_clock::now();

    // 1. Initial validation
    auto val_res = graph_.validate();
    if (!val_res.is_valid()) {
        std::cerr << "Pre-simulation Validation Error: " << val_res.error_message << "\n";
        return;
    }

    // 2. Schedule initial workload arrival events (Phase 5)
    for (const auto& [id, task] : graph_.get_tasks()) {
        schedule_event(task.arrival_tick, EventPhase::Phase5_WorkloadArrival,
                       EventType::WORKLOAD_ARRIVAL, id, DeviceId{0}, SlotId{0}, DmaEngineId{0}, 0, "Initial task arrival");
    }

    // 3. Main Discrete-Event Simulation Loop
    while (!event_queue_.empty() || !ready_tasks_.empty() || !ready_compute_tasks_.empty() || transfer_engine_.has_queued_transfers()) {
        if (event_queue_.empty()) {
            // Check for Deadlock condition (P0 fix)
            bool work_in_flight = (resources_.get_active_slots(DeviceType::CPU) > 0) ||
                                  (resources_.get_active_slots(DeviceType::GPU) > 0) ||
                                  (transfer_engine_.get_active_transfers() > 0);
            if (!work_in_flight) {
                // No future events scheduled and no resources active, but uncompleted tasks remain!
                deadlocked_ = true;
                metrics_.record_event(current_tick_, EventPhase::Phase6_DispatchDrain,
                                      EventType::SIMULATION_DEADLOCK, TaskId{0}, DeviceType::CPU, 0,
                                      "Permanent simulation deadlock detected. No progress possible.");
                break;
            }
        }

        if (event_queue_.empty()) {
            break;
        }

        // Advance simulation time to next scheduled event tick
        SimTick next_tick = event_queue_.top().tick;
        assert(next_tick >= current_tick_ && "Simulation time cannot move backward!");
        current_tick_ = next_tick;

        process_tick_events(current_tick_);
    }

    auto wall_end = std::chrono::steady_clock::now();
    SimTick wall_clock_us = static_cast<SimTick>(
        std::chrono::duration_cast<std::chrono::microseconds>(wall_end - wall_start).count());

    // Finalize metrics
    metrics_.finalize(current_tick_, graph_, resources_, config_.dma_engines,
                      total_dma_busy_ticks_, total_cpu_busy_ticks_, total_gpu_busy_ticks_, wall_clock_us);
}

void EventEngine::process_tick_events(SimTick tick) {
    // Collect all events scheduled for current_tick_ bucketed by phase
    std::vector<Event> p1_events;
    std::vector<Event> p2_events;
    std::vector<Event> p3_events;
    std::vector<Event> p4_events;
    std::vector<Event> p5_events;

    while (!event_queue_.empty() && event_queue_.top().tick == tick) {
        Event ev = event_queue_.top();
        event_queue_.pop();

        switch (ev.phase) {
            case EventPhase::Phase1_TransferComplete: p1_events.push_back(std::move(ev)); break;
            case EventPhase::Phase2_ComputeComplete: p2_events.push_back(std::move(ev)); break;
            case EventPhase::Phase3_MemoryRelease: p3_events.push_back(std::move(ev)); break;
            case EventPhase::Phase4_DependencyReadiness: p4_events.push_back(std::move(ev)); break;
            case EventPhase::Phase5_WorkloadArrival: p5_events.push_back(std::move(ev)); break;
            case EventPhase::Phase6_DispatchDrain: break;
        }
    }

    // Execute phases sequentially
    process_phase1_transfers(p1_events);
    process_phase2_completions(p2_events);
    process_phase3_memory_releases(p3_events);
    process_phase4_dependency_updates(p4_events);
    process_phase5_arrivals(p5_events);
    process_phase6_dispatch_drain();
}

void EventEngine::process_phase1_transfers(const std::vector<Event>& events) {
    for (const auto& ev : events) {
        auto* task = graph_.get_task_mut(ev.task_id);
        if (!task) continue;

        // Release occupied DMA engine back to pool
        transfer_engine_.release_dma_engine(ev.dma_id);

        task->transfer_end_tick = current_tick_;
        task->transition_to(TaskState::READY_COMPUTE);

        metrics_.record_event(current_tick_, EventPhase::Phase1_TransferComplete,
                              EventType::TRANSFER_COMPLETE, task->id, DeviceType::GPU,
                              ev.dma_id.value, "H2D PCIe DMA transfer completed");

        // Now that input data is resident, task is immediately eligible for compute dispatch
        dispatch_compute(task->id, DeviceType::GPU);
    }

    // Check if waiting transfers in DMA queue can now start on released engine
    check_dma_queue();
}

void EventEngine::process_phase2_completions(const std::vector<Event>& events) {
    for (const auto& ev : events) {
        auto* task = graph_.get_task_mut(ev.task_id);
        if (!task) continue;

        DeviceType dev = (task->assigned_device.value == 1) ? DeviceType::GPU : DeviceType::CPU;

        // Release compute execution slot back to pool
        resources_.release_compute_slot(dev, task->assigned_slot);

        task->finish_tick = current_tick_;
        task->transition_to(TaskState::COMPLETED);

        metrics_.record_event(current_tick_, EventPhase::Phase2_ComputeComplete,
                              EventType::COMPUTE_COMPLETE, task->id, dev,
                              task->assigned_slot.value, "Compute kernel finished");

        // Schedule Phase 3 memory release for this task
        schedule_event(current_tick_, EventPhase::Phase3_MemoryRelease,
                       EventType::MEMORY_RELEASE, task->id, task->assigned_device,
                       SlotId{0}, DmaEngineId{0}, task->working_bytes, "Working memory deallocation");

        // Schedule Phase 4 dependency resolution for successors
        schedule_event(current_tick_, EventPhase::Phase4_DependencyReadiness,
                       EventType::DEPENDENCY_RESOLVED, task->id, task->assigned_device,
                       SlotId{0}, DmaEngineId{0}, 0, "Unblock child dependencies");
    }

    drain_ready_compute_queue();
}

void EventEngine::process_phase3_memory_releases(const std::vector<Event>& events) {
    for (const auto& ev : events) {
        auto* task = graph_.get_task_mut(ev.task_id);
        if (!task) continue;

        DeviceType dev = (task->assigned_device.value == 1) ? DeviceType::GPU : DeviceType::CPU;

        // Reclaim input buffer and scratch/working memory
        resources_.release_working_memory(dev, task->input_bytes + task->working_bytes);

        // Publish output buffer with initial ref count equal to dependent child count
        resources_.publish_output_buffer(dev, task->id, task->output_bytes, task->successors.size());

        // For each dependency of this task that was on the same device, decrement parent buffer ref count
        for (const auto& parent_id : task->dependencies) {
            resources_.decrement_buffer_consumer(parent_id);
        }

        metrics_.record_event(current_tick_, EventPhase::Phase3_MemoryRelease,
                              EventType::MEMORY_RELEASE, task->id, dev, 0,
                              "Reclaimed working memory and published output buffer");
    }
}

void EventEngine::process_phase4_dependency_updates(const std::vector<Event>& events) {
    for (const auto& ev : events) {
        auto* parent = graph_.get_task(ev.task_id);
        if (!parent) continue;

        for (const auto& succ_id : parent->successors) {
            auto* child = graph_.get_task_mut(succ_id);
            if (!child) continue;

            if (child->pending_dependencies > 0) {
                child->pending_dependencies--;
            }

            // If all dependencies completed and task has already arrived
            if (child->pending_dependencies == 0 && child->state == TaskState::WAITING_DEPS) {
                child->transition_to(TaskState::READY);
                ready_tasks_.push_back(child->id);

                metrics_.record_event(current_tick_, EventPhase::Phase4_DependencyReadiness,
                                      EventType::DEPENDENCY_RESOLVED, child->id, DeviceType::CPU, 0,
                                      "All dependencies satisfied; moved to READY queue");
            }
        }
    }
}

void EventEngine::process_phase5_arrivals(const std::vector<Event>& events) {
    for (const auto& ev : events) {
        auto* task = graph_.get_task_mut(ev.task_id);
        if (!task) continue;

        if (task->pending_dependencies == 0) {
            task->transition_to(TaskState::READY);
            ready_tasks_.push_back(task->id);
            metrics_.record_event(current_tick_, EventPhase::Phase5_WorkloadArrival,
                                  EventType::WORKLOAD_ARRIVAL, task->id, DeviceType::CPU, 0,
                                  "Arrived with zero dependencies; moved directly to READY");
        } else {
            task->transition_to(TaskState::WAITING_DEPS);
            metrics_.record_event(current_tick_, EventPhase::Phase5_WorkloadArrival,
                                  EventType::WORKLOAD_ARRIVAL, task->id, DeviceType::CPU, 0,
                                  "Arrived; waiting for " + std::to_string(task->pending_dependencies) + " DAG dependencies");
        }
    }
}

void EventEngine::process_phase6_dispatch_drain() {
    drain_ready_compute_queue();

    // Exhaustive Dispatch Drain Loop
    while (!ready_tasks_.empty()) {
        auto decision = policy_->select_next_task(ready_tasks_, graph_, resources_, transfer_engine_, current_tick_);
        if (!decision.has_value()) {
            break; // No currently feasible ready candidate
        }

        TaskId task_id = decision->task_id;
        auto* task = graph_.get_task_mut(task_id);
        if (!task) break;

        // Remove from ready queue
        std::erase(ready_tasks_, task_id);

        DeviceType target_dev = decision->target_device;
        task->assigned_device = DeviceId{static_cast<std::uint32_t>(target_dev == DeviceType::GPU ? 1 : 0)};
        task->scheduled_tick = current_tick_;

        // Admission & Memory Pre-reservation
        ByteCount total_mem = task->input_bytes + task->working_bytes + task->output_bytes;
        if (!resources_.reserve_memory(target_dev, total_mem)) {
            // Should not happen if policy checked feasibility, but safeguard:
            task->blocked_reason = "Out of VRAM capacity";
            ready_tasks_.push_back(task_id);
            break;
        }

        if (decision->requires_transfer) {
            // Enqueue in DMA Transfer Queue (P0 Fix: Do NOT reserve compute slot yet!)
            task->transition_to(TaskState::QUEUED_DMA);
            transfer_engine_.enqueue_transfer(task->id);

            metrics_.record_event(current_tick_, EventPhase::Phase6_DispatchDrain,
                                  EventType::DISPATCH_TRANSFER, task->id, target_dev, 0,
                                  "Enqueued in PCIe DMA Transfer Queue");

            check_dma_queue();
        } else {
            // Data is already resident or CPU->CPU; move directly to READY_COMPUTE
            task->transition_to(TaskState::READY_COMPUTE);
            dispatch_compute(task->id, target_dev);
        }
    }

    drain_ready_compute_queue();
}

void EventEngine::check_dma_queue() {
    while (transfer_engine_.has_queued_transfers() && transfer_engine_.has_available_engine()) {
        TaskId task_id = transfer_engine_.pop_queued_transfer();
        auto* task = graph_.get_task_mut(task_id);
        if (!task) continue;

        auto dma_opt = transfer_engine_.acquire_dma_engine();
        if (!dma_opt.has_value()) break;

        task->assigned_dma = *dma_opt;
        task->transfer_start_tick = current_tick_;
        SimTick dur = transfer_engine_.calculate_transfer_duration(task->input_bytes);
        task->transfer_end_tick = current_tick_ + dur;
        task->transition_to(TaskState::TRANSFERRING);

        total_dma_busy_ticks_ += dur;

        metrics_.record_event(current_tick_, EventPhase::Phase6_DispatchDrain,
                              EventType::DISPATCH_TRANSFER, task->id, DeviceType::GPU,
                              task->assigned_dma.value, "Started PCIe DMA transfer (" + std::to_string(dur) + " us)");

        schedule_event(current_tick_ + dur, EventPhase::Phase1_TransferComplete,
                       EventType::TRANSFER_COMPLETE, task->id, DeviceId{1}, SlotId{0},
                       task->assigned_dma, task->input_bytes, "DMA transfer completion");
    }
}

void EventEngine::dispatch_compute(TaskId task_id, DeviceType device) {
    auto* task = graph_.get_task_mut(task_id);
    if (!task) return;

    if (resources_.has_available_slot(device)) {
        auto slot_opt = resources_.acquire_compute_slot(device);
        if (slot_opt.has_value()) {
            task->assigned_slot = *slot_opt;
            task->compute_start_tick = current_tick_;
            SimTick compute_dur = task->get_compute_duration(device);
            task->finish_tick = current_tick_ + compute_dur;
            task->transition_to(TaskState::RUNNING);

            if (device == DeviceType::CPU) {
                total_cpu_busy_ticks_ += compute_dur;
            } else {
                total_gpu_busy_ticks_ += compute_dur;
            }

            metrics_.record_event(current_tick_, EventPhase::Phase6_DispatchDrain,
                                  EventType::DISPATCH_COMPUTE, task->id, device,
                                  task->assigned_slot.value, "Compute kernel dispatched on slot " + std::to_string(task->assigned_slot.value));

            schedule_event(current_tick_ + compute_dur, EventPhase::Phase2_ComputeComplete,
                           EventType::COMPUTE_COMPLETE, task->id, task->assigned_device,
                           task->assigned_slot, DmaEngineId{0}, task->working_bytes, "Compute execution complete");
            return;
        }
    }

    // No slot available right now: enqueue in ready_compute_tasks_
    if (std::find(ready_compute_tasks_.begin(), ready_compute_tasks_.end(), task_id) == ready_compute_tasks_.end()) {
        ready_compute_tasks_.push_back(task_id);
    }
}

void EventEngine::drain_ready_compute_queue() {
    for (auto it = ready_compute_tasks_.begin(); it != ready_compute_tasks_.end(); ) {
        TaskId tid = *it;
        auto* task = graph_.get_task_mut(tid);
        if (task && task->state == TaskState::READY_COMPUTE) {
            DeviceType dev = (task->assigned_device.value == 1) ? DeviceType::GPU : DeviceType::CPU;
            if (resources_.has_available_slot(dev)) {
                it = ready_compute_tasks_.erase(it);
                dispatch_compute(tid, dev);
                continue;
            }
        }
        ++it;
    }
}

} // namespace heterosched
