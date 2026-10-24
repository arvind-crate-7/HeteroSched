#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/task.hpp"
#include "nlohmann/json.hpp"
#include <string>
#include <optional>

namespace heterosched {

struct SimulatorConfig {
    std::size_t cpu_slots{4};
    std::size_t gpu_slots{2};
    std::size_t dma_engines{1};
    ByteCount gpu_vram_bytes{16ULL * 1024 * 1024 * 1024}; // 16 GB default

    // PCIe interconnect bandwidth (bytes per SimTick = bytes/us => 16 GB/s = 16,000 bytes/us)
    ByteCount pcie_bandwidth_bytes_per_tick{16000};
    SimTick pcie_setup_latency_ticks{5}; // 5 us setup

    std::uint32_t random_seed{42};
};

class JsonSchema {
public:
    // Workload JSON parsing
    static bool parse_workload_string(const std::string& json_str, TaskGraph& out_graph, SimulatorConfig& out_config, std::string& out_error);
    static bool parse_workload_file(const std::string& file_path, TaskGraph& out_graph, SimulatorConfig& out_config, std::string& out_error);

    // Serialization helper
    static nlohmann::json to_json(const Task& task);
};

} // namespace heterosched
