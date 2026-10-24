#pragma once

#include "heterosched/types.hpp"
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace heterosched {

struct ResidentBuffer {
    TaskId producer_id{0};
    ByteCount bytes{0};
    std::size_t consumer_ref_count{0};
    DeviceType resident_device{DeviceType::GPU};
};

class ResourcePool {
public:
    ResourcePool(std::size_t cpu_slots, std::size_t gpu_slots, ByteCount gpu_vram_capacity);

    // Compute slot allocation
    [[nodiscard]] bool has_available_slot(DeviceType device) const noexcept;
    [[nodiscard]] std::optional<SlotId> acquire_compute_slot(DeviceType device);
    void release_compute_slot(DeviceType device, SlotId slot);

    [[nodiscard]] std::size_t get_active_slots(DeviceType device) const noexcept;
    [[nodiscard]] std::size_t get_max_slots(DeviceType device) const noexcept;

    // Memory capacity & pre-reservation (P0 fix)
    [[nodiscard]] bool can_admit_memory(DeviceType device, ByteCount total_needed) const noexcept;
    [[nodiscard]] bool is_permanently_infeasible(DeviceType device, ByteCount total_needed) const noexcept;
    bool reserve_memory(DeviceType device, ByteCount total_needed);
    void release_working_memory(DeviceType device, ByteCount working_bytes);

    // Output buffer publication & consumer reference counting
    void publish_output_buffer(DeviceType device, TaskId producer_id, ByteCount output_bytes, std::size_t initial_consumers);
    void decrement_buffer_consumer(TaskId producer_id);
    [[nodiscard]] bool is_buffer_resident(TaskId producer_id, DeviceType device) const noexcept;

    // Metrics & High-water marks
    [[nodiscard]] ByteCount get_gpu_vram_capacity() const noexcept { return gpu_vram_capacity_; }
    [[nodiscard]] ByteCount get_gpu_vram_allocated() const noexcept { return gpu_vram_allocated_; }
    [[nodiscard]] ByteCount get_peak_gpu_vram() const noexcept { return peak_gpu_vram_; }

private:
    std::vector<bool> cpu_slots_;
    std::vector<bool> gpu_slots_;

    ByteCount gpu_vram_capacity_{0};
    ByteCount gpu_vram_allocated_{0}; // Currently reserved + resident bytes
    ByteCount peak_gpu_vram_{0};

    std::unordered_map<TaskId, ResidentBuffer> resident_buffers_;
};

} // namespace heterosched
