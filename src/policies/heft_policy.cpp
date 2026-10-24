#include "heterosched/scheduling_policy.hpp"
#include <algorithm>
#include <unordered_map>
#include <limits>

namespace heterosched {

class HeftPolicy : public SchedulingPolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Full_HEFT_Upward_Rank";
    }

    std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) override {

        // Compute upward ranks if not already cached
        if (ranks_.empty()) {
            compute_upward_ranks(graph, transfer_engine);
        }

        // Sort ready candidates by rank_u DESC, then TaskId ASC
        std::vector<TaskId> candidates = ready_tasks;
        std::sort(candidates.begin(), candidates.end(), [&](TaskId a, TaskId b) {
            double ra = ranks_.contains(a) ? ranks_.at(a) : 0.0;
            double rb = ranks_.contains(b) ? ranks_.at(b) : 0.0;
            if (ra != rb) return ra > rb; // Critical path first
            return a.value < b.value;
        });

        for (TaskId id : candidates) {
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

            std::optional<SimTick> cpu_eft;
            if (cpu_eligible && resources.has_available_slot(DeviceType::CPU)) {
                cpu_eft = current_tick + task->cpu_compute_duration;
            }

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
                continue;
            }

            // Assign to processor minimizing EFT
            DeviceType chosen_dev = DeviceType::CPU;
            SimTick chosen_eft = 0;
            bool chosen_transfer = false;

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

            return ScheduleDecision{
                .task_id = id,
                .target_device = chosen_dev,
                .requires_transfer = chosen_transfer,
                .transfer_bytes = chosen_transfer ? task->input_bytes : 0,
                .estimated_finish_tick = chosen_eft,
                .score = ranks_.contains(id) ? ranks_.at(id) : 0.0
            };
        }

        return std::nullopt;
    }

private:
    void compute_upward_ranks(const TaskGraph& graph, const TransferEngine& transfer_engine) {
        ranks_.clear();
        const auto& order = graph.get_topological_order();

        // Traverse in reverse topological order (bottom-up from terminal exit nodes)
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            TaskId id = *it;
            const auto* task = graph.get_task(id);
            if (!task) continue;

            double avg_compute = 0.0;
            if (task->eligibility == DeviceEligibility::CPU_ONLY) {
                avg_compute = static_cast<double>(task->cpu_compute_duration);
            } else if (task->eligibility == DeviceEligibility::GPU_ONLY) {
                avg_compute = static_cast<double>(task->gpu_compute_duration);
            } else {
                avg_compute = (static_cast<double>(task->cpu_compute_duration) + static_cast<double>(task->gpu_compute_duration)) / 2.0;
            }

            double max_succ_cost = 0.0;
            for (const auto& succ_id : task->successors) {
                const auto* succ_task = graph.get_task(succ_id);
                double comm_cost = 0.0;
                if (succ_task) {
                    comm_cost = static_cast<double>(transfer_engine.calculate_transfer_duration(succ_task->input_bytes));
                }
                double succ_rank = ranks_.contains(succ_id) ? ranks_.at(succ_id) : 0.0;
                if ((comm_cost + succ_rank) > max_succ_cost) {
                    max_succ_cost = comm_cost + succ_rank;
                }
            }

            ranks_[id] = avg_compute + max_succ_cost;
        }
    }

    std::unordered_map<TaskId, double> ranks_;
};

std::unique_ptr<SchedulingPolicy> create_heft_policy() {
    return std::make_unique<HeftPolicy>();
}

} // namespace heterosched
