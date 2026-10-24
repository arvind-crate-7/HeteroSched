#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/resource_pool.hpp"
#include "heterosched/transfer_engine.hpp"
#include <string_view>
#include <vector>
#include <optional>
#include <memory>

namespace heterosched {

struct ScheduleDecision {
    TaskId task_id{0};
    DeviceType target_device{DeviceType::CPU};
    bool requires_transfer{false};
    ByteCount transfer_bytes{0};
    SimTick estimated_finish_tick{0};
    double score{0.0};
};

class SchedulingPolicy {
public:
    virtual ~SchedulingPolicy() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    // Evaluates ready tasks and returns the next feasible candidate to dispatch
    virtual std::optional<ScheduleDecision> select_next_task(
        const std::vector<TaskId>& ready_tasks,
        TaskGraph& graph,
        const ResourcePool& resources,
        const TransferEngine& transfer_engine,
        SimTick current_tick) = 0;
};

} // namespace heterosched
