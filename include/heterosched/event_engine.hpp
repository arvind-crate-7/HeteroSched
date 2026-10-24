#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/resource_pool.hpp"
#include "heterosched/transfer_engine.hpp"
#include "heterosched/event.hpp"
#include "heterosched/scheduling_policy.hpp"
#include "heterosched/metrics.hpp"
#include "heterosched/json_schema.hpp"
#include <queue>
#include <memory>
#include <vector>

namespace heterosched {

class EventEngine {
public:
    EventEngine(TaskGraph graph, SimulatorConfig config, std::unique_ptr<SchedulingPolicy> policy);

    // Run simulation to completion (or deadlock detection)
    void run();

    [[nodiscard]] SimTick get_current_tick() const noexcept { return current_tick_; }
    [[nodiscard]] const TaskGraph& get_graph() const noexcept { return graph_; }
    [[nodiscard]] const MetricsAggregator& get_metrics() const noexcept { return metrics_; }
    [[nodiscard]] const ResourcePool& get_resources() const noexcept { return resources_; }
    [[nodiscard]] const SimulatorConfig& get_config() const noexcept { return config_; }

    [[nodiscard]] bool is_deadlocked() const noexcept { return deadlocked_; }

private:
    void schedule_event(SimTick tick, EventPhase phase, EventType type, TaskId task_id,
                        DeviceId dev = DeviceId{0}, SlotId slot = SlotId{0},
                        DmaEngineId dma = DmaEngineId{0}, ByteCount bytes = 0, std::string meta = "");

    void process_tick_events(SimTick tick);

    // Intra-tick Phase Handlers
    void process_phase1_transfers(const std::vector<Event>& events);
    void process_phase2_completions(const std::vector<Event>& events);
    void process_phase3_memory_releases(const std::vector<Event>& events);
    void process_phase4_dependency_updates(const std::vector<Event>& events);
    void process_phase5_arrivals(const std::vector<Event>& events);
    void process_phase6_dispatch_drain();

    void check_dma_queue();
    void dispatch_compute(TaskId task_id, DeviceType device);
    void drain_ready_compute_queue();

    TaskGraph graph_;
    SimulatorConfig config_;
    ResourcePool resources_;
    TransferEngine transfer_engine_;
    std::unique_ptr<SchedulingPolicy> policy_;
    MetricsAggregator metrics_;

    std::priority_queue<Event, std::vector<Event>, EventComparator> event_queue_;
    std::vector<TaskId> ready_tasks_;
    std::vector<TaskId> ready_compute_tasks_;

    SimTick current_tick_{0};
    std::uint64_t sequence_counter_{0};
    bool deadlocked_{false};

    SimTick total_cpu_busy_ticks_{0};
    SimTick total_gpu_busy_ticks_{0};
    SimTick total_dma_busy_ticks_{0};
};

} // namespace heterosched
