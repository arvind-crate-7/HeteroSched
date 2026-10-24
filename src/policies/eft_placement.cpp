#include "heterosched/scheduling_policy.hpp"
#include <algorithm>
#include <limits>

namespace heterosched {

class EftPlacementPolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "EFT_Placement_Heuristic";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) override {

        std::optional<ScheduleDecision> best_decision;
        SimTick best_eft = std::numeric_limits<SimTick>::max();

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

            // 1. Evaluate CPU option
            std::optional<SimTick> cpu_eft;
            if (cpu_eligible && resources.has_available_slot(DeviceType::CPU)) {
                cpu_eft = current_tick + task->cpu_compute_duration;
            }

            // 2. Evaluate GPU option
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

            // 3. Choose device with minimum EFT
            if (cpu_eft.has_value() && gpu_eft.has_value()) {
                if (*gpu_eft < *cpu_eft) {
                    if (*gpu_eft < best_eft) {
                        best_eft = *gpu_eft;
                        best_decision = ScheduleDecision{
                            .task_id = id,
                            .target_device = DeviceType::GPU,
                            .requires_transfer = gpu_need_transfer,
                            .transfer_bytes = gpu_need_transfer ? task->input_bytes : 0,
                            .estimated_finish_tick = *gpu_eft,
                            .score = static_cast<double>(*gpu_eft)
                        };
                    }
                } else {
                    if (*cpu_eft < best_eft) {
                        best_eft = *cpu_eft;
                        best_decision = ScheduleDecision{
                            .task_id = id,
                            .target_device = DeviceType::CPU,
                            .requires_transfer = false,
                            .transfer_bytes = 0,
                            .estimated_finish_tick = *cpu_eft,
                            .score = static_cast<double>(*cpu_eft)
                        };
                    }
                }
            } else if (cpu_eft.has_value()) {
                if (*cpu_eft < best_eft) {
                    best_eft = *cpu_eft;
                    best_decision = ScheduleDecision{
                        .task_id = id,
                        .target_device = DeviceType::CPU,
                        .requires_transfer = false,
                        .transfer_bytes = 0,
                        .estimated_finish_tick = *cpu_eft,
                        .score = static_cast<double>(*cpu_eft)
                    };
                }
            } else if (gpu_eft.has_value()) {
                if (*gpu_eft < best_eft) {
                    best_eft = *gpu_eft;
                    best_decision = ScheduleDecision{
                        .task_id = id,
                        .target_device = DeviceType::GPU,
                        .requires_transfer = gpu_need_transfer,
                        .transfer_bytes = gpu_need_transfer ? task->input_bytes : 0,
                        .estimated_finish_tick = *gpu_eft,
                        .score = static_cast<double>(*gpu_eft)
                    };
                }
            }
        }

        return best_decision;
    }
};

std::unique_ptr<SchedulingPolicy> create_eft_placement_policy() {
    return std::make_unique<EftPlacementPolicy>();
}

} // namespace heterosched
