#include "heterosched/resource_pool.hpp"
#include <algorithm>

namespace heterosched {

ResourcePool::ResourcePool(std::size_t cpu_slots, std::size_t gpu_slots, ByteCount gpu_vram_capacity)
    : cpu_slots_(cpu_slots, false),
      gpu_slots_(gpu_slots, false),
      gpu_vram_capacity_(gpu_vram_capacity) {}

bool ResourcePool::has_available_slot(DeviceType device) const noexcept {
    const auto& slots = (device == DeviceType::CPU) ? cpu_slots_ : gpu_slots_;
    return std::any_of(slots.begin(), slots.end(), [](bool occ) { return !occ; });
}

std::optional<SlotId> ResourcePool::acquire_compute_slot(DeviceType device) {
    auto& slots = (device == DeviceType::CPU) ? cpu_slots_ : gpu_slots_;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i]) {
            slots[i] = true;
            return SlotId{static_cast<std::uint32_t>(i)};
        }
    }
    return std::nullopt;
}

void ResourcePool::release_compute_slot(DeviceType device, SlotId slot) {
    auto& slots = (device == DeviceType::CPU) ? cpu_slots_ : gpu_slots_;
    if (slot.value < slots.size()) {
        slots[slot.value] = false;
    }
}

std::size_t ResourcePool::get_active_slots(DeviceType device) const noexcept {
    const auto& slots = (device == DeviceType::CPU) ? cpu_slots_ : gpu_slots_;
    return static_cast<std::size_t>(std::count(slots.begin(), slots.end(), true));
}

std::size_t ResourcePool::get_max_slots(DeviceType device) const noexcept {
    return (device == DeviceType::CPU) ? cpu_slots_.size() : gpu_slots_.size();
}

bool ResourcePool::can_admit_memory(DeviceType device, ByteCount total_needed) const noexcept {
    if (device == DeviceType::CPU) {
        return true; // Host RAM assumed unbounded for abstract single-node
    }
    return (gpu_vram_allocated_ + total_needed) <= gpu_vram_capacity_;
}

bool ResourcePool::is_permanently_infeasible(DeviceType device, ByteCount total_needed) const noexcept {
    if (device == DeviceType::CPU) return false;
    return total_needed > gpu_vram_capacity_;
}

bool ResourcePool::reserve_memory(DeviceType device, ByteCount total_needed) {
    if (device == DeviceType::CPU) return true;

    if (!can_admit_memory(device, total_needed)) {
        return false;
    }

    gpu_vram_allocated_ += total_needed;
    if (gpu_vram_allocated_ > peak_gpu_vram_) {
        peak_gpu_vram_ = gpu_vram_allocated_;
    }
    return true;
}

void ResourcePool::release_working_memory(DeviceType device, ByteCount working_bytes) {
    if (device == DeviceType::CPU) return;

    if (working_bytes > gpu_vram_allocated_) {
        gpu_vram_allocated_ = 0;
    } else {
        gpu_vram_allocated_ -= working_bytes;
    }
}

void ResourcePool::publish_output_buffer(DeviceType device, TaskId producer_id, ByteCount output_bytes, std::size_t initial_consumers) {
    if (device == DeviceType::CPU || output_bytes == 0 || initial_consumers == 0) {
        // If 0 consumers, immediately reclaim any output reservation
        if (device == DeviceType::GPU && output_bytes > 0 && initial_consumers == 0) {
            if (output_bytes > gpu_vram_allocated_) gpu_vram_allocated_ = 0;
            else gpu_vram_allocated_ -= output_bytes;
        }
        return;
    }

    resident_buffers_[producer_id] = ResidentBuffer{
        .producer_id = producer_id,
        .bytes = output_bytes,
        .consumer_ref_count = initial_consumers,
        .resident_device = device
    };
}

void ResourcePool::decrement_buffer_consumer(TaskId producer_id) {
    auto it = resident_buffers_.find(producer_id);
    if (it == resident_buffers_.end()) return;

    if (it->second.consumer_ref_count > 0) {
        it->second.consumer_ref_count--;
    }

    if (it->second.consumer_ref_count == 0) {
        // All consumers finished. Reclaim memory immediately in Phase 3
        if (it->second.resident_device == DeviceType::GPU) {
            if (it->second.bytes > gpu_vram_allocated_) {
                gpu_vram_allocated_ = 0;
            } else {
                gpu_vram_allocated_ -= it->second.bytes;
            }
        }
        resident_buffers_.erase(it);
    }
}

bool ResourcePool::is_buffer_resident(TaskId producer_id, DeviceType device) const noexcept {
    auto it = resident_buffers_.find(producer_id);
    if (it == resident_buffers_.end()) return false;
    return it->second.resident_device == device;
}

} // namespace heterosched
