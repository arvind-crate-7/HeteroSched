#include "heterosched/scheduling_policy.hpp"
#include <algorithm>

namespace heterosched {

class FeasibleFifoPolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Feasible_FIFO_Baseline";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) override {

        // Sort ready candidates strictly by (ArrivalTick ASC, TaskId ASC)
        std::vector<TaskId> candidates = ready_tasks;
        std::sort(candidates.begin(), candidates.end(), [&](TaskId a, TaskId b) {
            const auto* ta = graph.get_task(a);
            const auto* tb = graph.get_task(b);
            if (ta->arrival_tick != tb->arrival_tick) return ta->arrival_tick < tb->arrival_tick;
            return a.value < b.value;
        });

        for (TaskId id : candidates) {
            auto* task = graph.get_task_mut(id);
            if (!task) continue;

            ByteCount total_mem = task->input_bytes + task->working_bytes + task->output_bytes;

            // Infeasibility check: Cannot run on either device
            bool cpu_eligible = task->is_eligible_for(DeviceType::CPU);
            bool gpu_eligible = task->is_eligible_for(DeviceType::GPU) && !resources.is_permanently_infeasible(DeviceType::GPU, total_mem);

            if (!cpu_eligible && !gpu_eligible) {
                task->transition_to(TaskState::INFEASIBLE);
                task->blocked_reason = "Cannot run on either CPU or GPU due to eligibility or VRAM capacity";
                continue;
            }

            // Check if CPU is feasible right now (has available slot)
            if (cpu_eligible && resources.has_available_slot(DeviceType::CPU)) {
                SimTick eft = current_tick + task->cpu_compute_duration;
                return ScheduleDecision{
                    .task_id = id,
                    .target_device = DeviceType::CPU,
                    .requires_transfer = false,
                    .transfer_bytes = 0,
                    .estimated_finish_tick = eft,
                    .score = static_cast<double>(eft)
                };
            }

            // Check if GPU is feasible right now (memory admission passes)
            if (gpu_eligible && resources.can_admit_memory(DeviceType::GPU, total_mem)) {
                bool need_transfer = (task->input_bytes > 0);
                if (need_transfer) {
                    bool all_resident = true;
                    for (const auto& parent_id : task->dependencies) {
                        if (!resources.is_buffer_resident(parent_id, DeviceType::GPU)) {
                            all_resident = false;
                            break;
                        }
                    }
                    if (all_resident && !task->dependencies.empty()) {
                        need_transfer = false;
                    }
                }

                SimTick transfer_dur = need_transfer ? transfer_engine.calculate_transfer_duration(task->input_bytes) : 0;
                SimTick eft = current_tick + transfer_dur + task->gpu_compute_duration;

                return ScheduleDecision{
                    .task_id = id,
                    .target_device = DeviceType::GPU,
                    .requires_transfer = need_transfer,
                    .transfer_bytes = need_transfer ? task->input_bytes : 0,
                    .estimated_finish_tick = eft,
                    .score = static_cast<double>(eft)
                };
            }
        }

        return std::nullopt;
    }
};

std::unique_ptr<SchedulingPolicy> create_fifo_policy() {
    return std::make_unique<FeasibleFifoPolicy>();
}

} // namespace heterosched
