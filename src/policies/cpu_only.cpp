#include "heterosched/scheduling_policy.hpp"

namespace heterosched {

class CpuOnlyPolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "CPU_Only_Baseline";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& /*transfer_engine*/,
        SimTick current_tick) override {

        for (TaskId id : ready_tasks) {
            auto* task = graph.get_task_mut(id);
            if (!task) continue;

            // Infeasible check
            if (!task->is_eligible_for(DeviceType::CPU)) {
                task->transition_to(TaskState::INFEASIBLE);
                task->blocked_reason = "Task requires GPU; ineligible on CPU-only baseline";
                continue;
            }

            // Feasibility check: CPU compute slot available
            if (resources.has_available_slot(DeviceType::CPU)) {
                SimTick est = current_tick;
                SimTick eft = est + task->cpu_compute_duration;
                return ScheduleDecision{
                    .task_id = id,
                    .target_device = DeviceType::CPU,
                    .requires_transfer = false,
                    .transfer_bytes = 0,
                    .estimated_finish_tick = eft,
                    .score = static_cast<double>(eft)
                };
            }
        }

        return std::nullopt;
    }
};

std::unique_ptr<SchedulingPolicy> create_cpu_only_policy() {
    return std::make_unique<CpuOnlyPolicy>();
}

} // namespace heterosched
