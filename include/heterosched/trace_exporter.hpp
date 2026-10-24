#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "nlohmann/json.hpp"
#include <string>

namespace heterosched {

class TraceExporter {
public:
    static nlohmann::json export_chrome_trace(const TaskGraph& graph, std::size_t cpu_slots, std::size_t gpu_slots, std::size_t dma_engines);
    static bool save_trace_file(const std::string& file_path, const TaskGraph& graph, std::size_t cpu_slots, std::size_t gpu_slots, std::size_t dma_engines);
};

} // namespace heterosched
