#pragma once

#include "heterosched/types.hpp"
#include <vector>
#include <queue>
#include <optional>
#include <algorithm>

namespace heterosched {

class TransferEngine {
public:
    TransferEngine(std::size_t num_engines, ByteCount bandwidth_bytes_per_tick, SimTick setup_latency_ticks);

    [[nodiscard]] bool has_available_engine() const noexcept;
    [[nodiscard]] std::optional<DmaEngineId> acquire_dma_engine();
    void release_dma_engine(DmaEngineId engine);

    [[nodiscard]] SimTick calculate_transfer_duration(ByteCount bytes) const noexcept;

    [[nodiscard]] std::size_t get_active_transfers() const noexcept;
    [[nodiscard]] std::size_t get_engine_count() const noexcept { return dma_engines_.size(); }

    // DMA Transfer Queue (P0 Decoupled Queueing)
    void enqueue_transfer(TaskId task_id);
    [[nodiscard]] bool has_queued_transfers() const noexcept { return !queue_.empty(); }
    [[nodiscard]] TaskId peek_queued_transfer() const { return queue_.front(); }
    TaskId pop_queued_transfer();
    [[nodiscard]] std::size_t queue_size() const noexcept { return queue_.size(); }

private:
    std::vector<bool> dma_engines_;
    ByteCount bandwidth_bytes_per_tick_{16000}; // 16 GB/s default
    SimTick setup_latency_ticks_{5};

    std::queue<TaskId> queue_;
};

} // namespace heterosched
