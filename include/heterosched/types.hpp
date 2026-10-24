#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <compare>
#include <functional>
#include <iostream>

namespace heterosched {

// Time representation: 1 tick = 1 microsecond (us)
using SimTick = std::uint64_t;
using ByteCount = std::uint64_t;

// Strongly typed IDs to avoid primitive confusion
struct TaskId {
    std::uint32_t value{0};

    constexpr TaskId() = default;
    constexpr explicit TaskId(std::uint32_t v) : value(v) {}

    constexpr auto operator<=>(const TaskId&) const = default;
    constexpr bool operator==(const TaskId&) const = default;
};

struct DeviceId {
    std::uint32_t value{0};

    constexpr DeviceId() = default;
    constexpr explicit DeviceId(std::uint32_t v) : value(v) {}

    constexpr auto operator<=>(const DeviceId&) const = default;
    constexpr bool operator==(const DeviceId&) const = default;
};

struct SlotId {
    std::uint32_t value{0};

    constexpr SlotId() = default;
    constexpr explicit SlotId(std::uint32_t v) : value(v) {}

    constexpr auto operator<=>(const SlotId&) const = default;
    constexpr bool operator==(const SlotId&) const = default;
};

struct DmaEngineId {
    std::uint32_t value{0};

    constexpr DmaEngineId() = default;
    constexpr explicit DmaEngineId(std::uint32_t v) : value(v) {}

    constexpr auto operator<=>(const DmaEngineId&) const = default;
    constexpr bool operator==(const DmaEngineId&) const = default;
};

enum class DeviceType : std::uint8_t {
    CPU = 0,
    GPU = 1
};

enum class DeviceEligibility : std::uint8_t {
    CPU_ONLY = 0,
    GPU_ONLY = 1,
    CPU_OR_GPU = 2
};

// Complete 10-state lifecycle FSM (7 intermediate + 3 runtime terminal)
enum class TaskState : std::uint8_t {
    UNSUBMITTED = 0,      // Registered in workload; arrival tick in the future
    WAITING_DEPS = 1,     // Arrived at current tick; waiting for DAG parents to complete
    READY = 2,            // All DAG parents completed; awaiting admission & placement
    QUEUED_DMA = 3,       // Placed on target device; enqueued in DMA queue for copy engine
    TRANSFERRING = 4,     // Actively occupying a DMA copy engine for PCIe data movement
    READY_COMPUTE = 5,    // Input data resident on destination; awaiting compute execution slot
    RUNNING = 6,          // Actively occupying a compute slot (CPU worker or GPU stream)
    COMPLETED = 7,        // Compute finished, working memory released, output buffer published
    INFEASIBLE = 8,       // Cannot be scheduled (memory exceeds physical capacity or device ineligible)
    FAILED_DEPENDENCY = 9 // Parent dependency failed or became infeasible
};

// 6 deterministic intra-tick event phases
enum class EventPhase : std::uint8_t {
    Phase1_TransferComplete = 1,
    Phase2_ComputeComplete = 2,
    Phase3_MemoryRelease = 3,
    Phase4_DependencyReadiness = 4,
    Phase5_WorkloadArrival = 5,
    Phase6_DispatchDrain = 6
};

enum class EventType : std::uint8_t {
    WORKLOAD_ARRIVAL,
    TRANSFER_COMPLETE,
    COMPUTE_COMPLETE,
    MEMORY_RELEASE,
    DEPENDENCY_RESOLVED,
    DISPATCH_TRANSFER,
    DISPATCH_COMPUTE,
    SIMULATION_DEADLOCK
};

// String conversions for logging, JSON serialization, and tracing
constexpr std::string_view to_string(DeviceType type) noexcept {
    switch (type) {
        case DeviceType::CPU: return "CPU";
        case DeviceType::GPU: return "GPU";
    }
    return "UNKNOWN";
}

constexpr std::string_view to_string(DeviceEligibility elig) noexcept {
    switch (elig) {
        case DeviceEligibility::CPU_ONLY: return "CPU_ONLY";
        case DeviceEligibility::GPU_ONLY: return "GPU_ONLY";
        case DeviceEligibility::CPU_OR_GPU: return "CPU_OR_GPU";
    }
    return "UNKNOWN";
}

constexpr std::string_view to_string(TaskState state) noexcept {
    switch (state) {
        case TaskState::UNSUBMITTED: return "UNSUBMITTED";
        case TaskState::WAITING_DEPS: return "WAITING_DEPS";
        case TaskState::READY: return "READY";
        case TaskState::QUEUED_DMA: return "QUEUED_DMA";
        case TaskState::TRANSFERRING: return "TRANSFERRING";
        case TaskState::READY_COMPUTE: return "READY_COMPUTE";
        case TaskState::RUNNING: return "RUNNING";
        case TaskState::COMPLETED: return "COMPLETED";
        case TaskState::INFEASIBLE: return "INFEASIBLE";
        case TaskState::FAILED_DEPENDENCY: return "FAILED_DEPENDENCY";
    }
    return "UNKNOWN";
}

constexpr std::string_view to_string(EventPhase phase) noexcept {
    switch (phase) {
        case EventPhase::Phase1_TransferComplete: return "Phase1_TransferComplete";
        case EventPhase::Phase2_ComputeComplete: return "Phase2_ComputeComplete";
        case EventPhase::Phase3_MemoryRelease: return "Phase3_MemoryRelease";
        case EventPhase::Phase4_DependencyReadiness: return "Phase4_DependencyReadiness";
        case EventPhase::Phase5_WorkloadArrival: return "Phase5_WorkloadArrival";
        case EventPhase::Phase6_DispatchDrain: return "Phase6_DispatchDrain";
    }
    return "UNKNOWN";
}

constexpr std::string_view to_string(EventType type) noexcept {
    switch (type) {
        case EventType::WORKLOAD_ARRIVAL: return "WORKLOAD_ARRIVAL";
        case EventType::TRANSFER_COMPLETE: return "TRANSFER_COMPLETE";
        case EventType::COMPUTE_COMPLETE: return "COMPUTE_COMPLETE";
        case EventType::MEMORY_RELEASE: return "MEMORY_RELEASE";
        case EventType::DEPENDENCY_RESOLVED: return "DEPENDENCY_RESOLVED";
        case EventType::DISPATCH_TRANSFER: return "DISPATCH_TRANSFER";
        case EventType::DISPATCH_COMPUTE: return "DISPATCH_COMPUTE";
        case EventType::SIMULATION_DEADLOCK: return "SIMULATION_DEADLOCK";
    }
    return "UNKNOWN";
}

} // namespace heterosched

// Hash specializations for std::unordered_map / set
template <>
struct std::hash<heterosched::TaskId> {
    std::size_t operator()(const heterosched::TaskId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<heterosched::DeviceId> {
    std::size_t operator()(const heterosched::DeviceId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<heterosched::SlotId> {
    std::size_t operator()(const heterosched::SlotId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<heterosched::DmaEngineId> {
    std::size_t operator()(const heterosched::DmaEngineId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
