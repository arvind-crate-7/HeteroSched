#pragma once

#include "heterosched/scheduling_policy.hpp"
#include <memory>
#include <string_view>

namespace heterosched {

std::unique_ptr<SchedulingPolicy> create_cpu_only_policy();
std::unique_ptr<SchedulingPolicy> create_gpu_only_policy();
std::unique_ptr<SchedulingPolicy> create_fifo_policy();
std::unique_ptr<SchedulingPolicy> create_eft_placement_policy();
std::unique_ptr<SchedulingPolicy> create_deadline_aware_policy();
std::unique_ptr<SchedulingPolicy> create_heft_policy();

inline std::unique_ptr<SchedulingPolicy> create_policy_by_name(std::string_view name) {
    if (name == "cpu_only" || name == "CPU_Only_Baseline") return create_cpu_only_policy();
    if (name == "gpu_only" || name == "GPU_Only_Baseline") return create_gpu_only_policy();
    if (name == "fifo" || name == "Feasible_FIFO_Baseline") return create_fifo_policy();
    if (name == "eft" || name == "EFT_Placement_Heuristic") return create_eft_placement_policy();
    if (name == "deadline_aware" || name == "Deadline_Aware_LST") return create_deadline_aware_policy();
    if (name == "heft" || name == "Full_HEFT_Upward_Rank") return create_heft_policy();
    return nullptr;
}

} // namespace heterosched
