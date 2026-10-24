#include "heterosched/scheduling_policy.hpp"

namespace heterosched {

class GpuOnlyPolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "GPU_Only_Baseline";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) override {

        for (TaskId id : ready_tasks) {
            auto* task = graph.get_task_mut(id);
            if (!task) continue;

            ByteCount total_mem = task->input_bytes + task->working_bytes + task->output_bytes;

            // Infeasibility check: Device eligibility or exceeds total VRAM
            if (!task->is_eligible_for(DeviceType::GPU) ||
                resources.is_permanently_infeasible(DeviceType::GPU, total_mem)) {
                task->transition_to(TaskState::INFEASIBLE);
                task->blocked_reason = "Ineligible or memory demand exceeds physical GPU VRAM capacity";
                continue;
            }

            // Admission check: Can GPU VRAM reserve memory right now?
            if (!resources.can_admit_memory(DeviceType::GPU, total_mem)) {
                task->blocked_reason = "Temporarily blocked waiting for GPU VRAM";
                continue;
            }

            // Check if transfer is required
            bool need_transfer = (task->input_bytes > 0);
            if (need_transfer) {
                // If parents were already on GPU, check residency
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
            SimTick est = current_tick + transfer_dur;
            SimTick eft = est + task->gpu_compute_duration;

            return ScheduleDecision{
                .task_id = id,
                .target_device = DeviceType::GPU,
                .requires_transfer = need_transfer,
                .transfer_bytes = need_transfer ? task->input_bytes : 0,
                .estimated_finish_tick = eft,
                .score = static_cast<double>(eft)
            };
        }

        return std::nullopt;
    }
};

std::unique_ptr<SchedulingPolicy> create_gpu_only_policy() {
    return std::make_unique<GpuOnlyPolicy>();
}

} // namespace heterosched
