#pragma once

#include "heterosched/types.hpp"
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <sstream>

namespace heterosched {

struct Task {
    TaskId id{0};
    std::string name;
    SimTick arrival_tick{0};
    DeviceEligibility eligibility{DeviceEligibility::CPU_OR_GPU};

    // Modeled execution costs (strictly positive >= 1 tick)
    SimTick cpu_compute_duration{1};
    SimTick gpu_compute_duration{1};

    // Memory footprints
    ByteCount input_bytes{0};
    ByteCount working_bytes{0};
    ByteCount output_bytes{0};

    // Real-time constraints & priority
    std::optional<SimTick> deadline;
    std::uint32_t priority{0};

    // Graph structure
    std::vector<TaskId> dependencies;
    std::vector<TaskId> successors;

    // Runtime state tracking
    TaskState state{TaskState::UNSUBMITTED};
    DeviceId assigned_device{0};
    SlotId assigned_slot{0};
    DmaEngineId assigned_dma{0};

    SimTick scheduled_tick{0};
    SimTick transfer_start_tick{0};
    SimTick transfer_end_tick{0};
    SimTick compute_start_tick{0};
    SimTick finish_tick{0};

    std::size_t pending_dependencies{0};
    std::string blocked_reason;

    // Checks if task is eligible to run on specified device type
    [[nodiscard]] constexpr bool is_eligible_for(DeviceType type) const noexcept {
        if (eligibility == DeviceEligibility::CPU_OR_GPU) return true;
        if (eligibility == DeviceEligibility::CPU_ONLY && type == DeviceType::CPU) return true;
        if (eligibility == DeviceEligibility::GPU_ONLY && type == DeviceType::GPU) return true;
        return false;
    }

    [[nodiscard]] constexpr SimTick get_compute_duration(DeviceType type) const noexcept {
        return (type == DeviceType::GPU) ? gpu_compute_duration : cpu_compute_duration;
    }

    [[nodiscard]] constexpr bool is_terminal() const noexcept {
        return state == TaskState::COMPLETED ||
               state == TaskState::INFEASIBLE ||
               state == TaskState::FAILED_DEPENDENCY;
    }

    // Strict 10-state lifecycle FSM transition with invariant validation
    void transition_to(TaskState new_state) {
        if (state == new_state) return;

        bool legal = false;
        switch (state) {
            case TaskState::UNSUBMITTED:
                legal = (new_state == TaskState::WAITING_DEPS || new_state == TaskState::READY);
                break;
            case TaskState::WAITING_DEPS:
                legal = (new_state == TaskState::READY || new_state == TaskState::FAILED_DEPENDENCY);
                break;
            case TaskState::READY:
                legal = (new_state == TaskState::QUEUED_DMA ||
                         new_state == TaskState::READY_COMPUTE ||
                         new_state == TaskState::INFEASIBLE ||
                         new_state == TaskState::FAILED_DEPENDENCY);
                break;
            case TaskState::QUEUED_DMA:
                legal = (new_state == TaskState::TRANSFERRING || new_state == TaskState::FAILED_DEPENDENCY);
                break;
            case TaskState::TRANSFERRING:
                legal = (new_state == TaskState::READY_COMPUTE || new_state == TaskState::FAILED_DEPENDENCY);
                break;
            case TaskState::READY_COMPUTE:
                legal = (new_state == TaskState::RUNNING || new_state == TaskState::FAILED_DEPENDENCY);
                break;
            case TaskState::RUNNING:
                legal = (new_state == TaskState::COMPLETED);
                break;
            case TaskState::COMPLETED:
            case TaskState::INFEASIBLE:
            case TaskState::FAILED_DEPENDENCY:
                legal = false; // Terminal states cannot transition
                break;
        }

        if (!legal) {
            std::ostringstream oss;
            oss << "Invariant Violation: Illegal TaskState transition for Task " << id.value
                << " from " << to_string(state) << " to " << to_string(new_state);
            throw std::logic_error(oss.str());
        }

        state = new_state;
    }
};

} // namespace heterosched
