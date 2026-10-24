#include "heterosched/transfer_engine.hpp"

namespace heterosched {

TransferEngine::TransferEngine(std::size_t num_engines, ByteCount bandwidth_bytes_per_tick, SimTick setup_latency_ticks)
    : dma_engines_(std::max<std::size_t>(1, num_engines), false),
      bandwidth_bytes_per_tick_(std::max<ByteCount>(1, bandwidth_bytes_per_tick)),
      setup_latency_ticks_(setup_latency_ticks) {}

bool TransferEngine::has_available_engine() const noexcept {
    return std::any_of(dma_engines_.begin(), dma_engines_.end(), [](bool occ) { return !occ; });
}

std::optional<DmaEngineId> TransferEngine::acquire_dma_engine() {
    for (std::size_t i = 0; i < dma_engines_.size(); ++i) {
        if (!dma_engines_[i]) {
            dma_engines_[i] = true;
            return DmaEngineId{static_cast<std::uint32_t>(i)};
        }
    }
    return std::nullopt;
}

void TransferEngine::release_dma_engine(DmaEngineId engine) {
    if (engine.value < dma_engines_.size()) {
        dma_engines_[engine.value] = false;
    }
}

SimTick TransferEngine::calculate_transfer_duration(ByteCount bytes) const noexcept {
    if (bytes == 0) return 0;
    // Ceiling division: ceil(bytes / bandwidth)
    SimTick transfer_ticks = (bytes + bandwidth_bytes_per_tick_ - 1) / bandwidth_bytes_per_tick_;
    return setup_latency_ticks_ + std::max<SimTick>(1, transfer_ticks);
}

std::size_t TransferEngine::get_active_transfers() const noexcept {
    return static_cast<std::size_t>(std::count(dma_engines_.begin(), dma_engines_.end(), true));
}

void TransferEngine::enqueue_transfer(TaskId task_id) {
    queue_.push(task_id);
}

TaskId TransferEngine::pop_queued_transfer() {
    TaskId t = queue_.front();
    queue_.pop();
    return t;
}

} // namespace heterosched
