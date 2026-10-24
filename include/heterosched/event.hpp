#pragma once

#include "heterosched/types.hpp"
#include <string>

namespace heterosched {

struct Event {
    SimTick tick{0};
    EventPhase phase{EventPhase::Phase5_WorkloadArrival};
    std::uint64_t sequence_number{0};

    EventType type{EventType::WORKLOAD_ARRIVAL};
    TaskId task_id{0};
    DeviceId device_id{0};
    SlotId slot_id{0};
    DmaEngineId dma_id{0};
    ByteCount bytes{0};
    std::string metadata;
};

// Strict deterministic Min-Heap ordering: (Tick ASC, Phase ASC, SequenceNumber ASC)
struct EventComparator {
    bool operator()(const Event& lhs, const Event& rhs) const noexcept {
        if (lhs.tick != rhs.tick) {
            return lhs.tick > rhs.tick;
        }
        if (lhs.phase != rhs.phase) {
            return static_cast<std::uint8_t>(lhs.phase) > static_cast<std::uint8_t>(rhs.phase);
        }
        return lhs.sequence_number > rhs.sequence_number;
    }
};

} // namespace heterosched
