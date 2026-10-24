#include "heterosched/scheduling_policy.hpp"
#include <algorithm>
#include <limits>

namespace heterosched {

class DeadlineAwarePolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Deadline_Aware_LST";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) override {

        std::optional<ScheduleDecision> best_decision;
        double best_urgency = -std::numeric_limits<double>::infinity(); // Higher urgency = selected

        for (TaskId id : ready_tasks) {
            auto* task = graph.get_task_mut(id);
            if (!task) continue;

            ByteCount total_mem = task->input_bytes + task->working_bytes + task->output_bytes;

            bool cpu_eligible = task->is_eligible_for(DeviceType::CPU);
            bool gpu_eligible = task->is_eligible_for(DeviceType::GPU) && !resources.is_permanently_infeasible(DeviceType::GPU, total_mem);

            if (!cpu_eligible && !gpu_eligible) {
                task->transition_to(TaskState::INFEASIBLE);
                task->blocked_reason = "Permanently infeasible on all devices";
                continue;
            }

            // Calculate EFT for CPU
            std::optional<SimTick> cpu_eft;
            if (cpu_eligible && resources.has_available_slot(DeviceType::CPU)) {
                cpu_eft = current_tick + task->cpu_compute_duration;
            }

            // Calculate EFT for GPU
            std::optional<SimTick> gpu_eft;
            bool gpu_need_transfer = false;
            if (gpu_eligible && resources.can_admit_memory(DeviceType::GPU, total_mem)) {
                gpu_need_transfer = (task->input_bytes > 0);
                if (gpu_need_transfer) {
                    bool all_resident = true;
                    for (const auto& parent_id : task->dependencies) {
                        if (!resources.is_buffer_resident(parent_id, DeviceType::GPU)) {
                            all_resident = false;
                            break;
                        }
                    }
                    if (all_resident && !task->dependencies.empty()) {
                        gpu_need_transfer = false;
                    }
                }
                SimTick transfer_dur = gpu_need_transfer ? transfer_engine.calculate_transfer_duration(task->input_bytes) : 0;
                gpu_eft = current_tick + transfer_dur + task->gpu_compute_duration;
            }

            if (!cpu_eft.has_value() && !gpu_eft.has_value()) {
                continue; // Neither device currently feasible
            }

            // Device selection & Urgency calculation
            DeviceType chosen_dev = DeviceType::CPU;
            SimTick chosen_eft = 0;
            bool chosen_transfer = false;

            if (task->deadline.has_value()) {
                SimTick dline = *task->deadline;

                // Dynamic Deadline Salvage: If CPU would miss deadline but GPU can meet it
                bool cpu_meets = cpu_eft.has_value() && (*cpu_eft <= dline);
                bool gpu_meets = gpu_eft.has_value() && (*gpu_eft <= dline);

                if (!cpu_meets && gpu_meets) {
                    chosen_dev = DeviceType::GPU;
                    chosen_eft = *gpu_eft;
                    chosen_transfer = gpu_need_transfer;
                } else if (cpu_eft.has_value() && gpu_eft.has_value()) {
                    // Both meet or both miss: choose lowest EFT
                    if (*gpu_eft < *cpu_eft) {
                        chosen_dev = DeviceType::GPU;
                        chosen_eft = *gpu_eft;
                        chosen_transfer = gpu_need_transfer;
                    } else {
                        chosen_dev = DeviceType::CPU;
                        chosen_eft = *cpu_eft;
                        chosen_transfer = false;
                    }
                } else if (gpu_eft.has_value()) {
                    chosen_dev = DeviceType::GPU;
                    chosen_eft = *gpu_eft;
                    chosen_transfer = gpu_need_transfer;
                } else {
                    chosen_dev = DeviceType::CPU;
                    chosen_eft = *cpu_eft;
                    chosen_transfer = false;
                }

                // Slack: Deadline - EFT
                double slack = static_cast<double>(static_cast<std::int64_t>(dline) - static_cast<std::int64_t>(chosen_eft));
                // Least positive slack has highest urgency (e.g. urgency = 1e9 - slack)
                double urgency = 1e9 - slack;

                if (urgency > best_urgency) {
                    best_urgency = urgency;
                    best_decision = ScheduleDecision{
                        .task_id = id,
                        .target_device = chosen_dev,
                        .requires_transfer = chosen_transfer,
                        .transfer_bytes = chosen_transfer ? task->input_bytes : 0,
                        .estimated_finish_tick = chosen_eft,
                        .score = slack
                    };
                }
            } else {
                // No deadline: fallback to EFT
                if (cpu_eft.has_value() && gpu_eft.has_value()) {
                    if (*gpu_eft < *cpu_eft) {
                        chosen_dev = DeviceType::GPU;
                        chosen_eft = *gpu_eft;
                        chosen_transfer = gpu_need_transfer;
                    } else {
                        chosen_dev = DeviceType::CPU;
                        chosen_eft = *cpu_eft;
                        chosen_transfer = false;
                    }
                } else if (gpu_eft.has_value()) {
                    chosen_dev = DeviceType::GPU;
                    chosen_eft = *gpu_eft;
                    chosen_transfer = gpu_need_transfer;
                } else {
                    chosen_dev = DeviceType::CPU;
                    chosen_eft = *cpu_eft;
                    chosen_transfer = false;
                }

                double urgency = -static_cast<double>(chosen_eft);
                if (urgency > best_urgency) {
                    best_urgency = urgency;
                    best_decision = ScheduleDecision{
                        .task_id = id,
                        .target_device = chosen_dev,
                        .requires_transfer = chosen_transfer,
                        .transfer_bytes = chosen_transfer ? task->input_bytes : 0,
                        .estimated_finish_tick = chosen_eft,
                        .score = -static_cast<double>(chosen_eft)
                    };
                }
            }
        }

        return best_decision;
    }
};

std::unique_ptr<SchedulingPolicy> create_deadline_aware_policy() {
    return std::make_unique<DeadlineAwarePolicy>();
}

} // namespace heterosched
