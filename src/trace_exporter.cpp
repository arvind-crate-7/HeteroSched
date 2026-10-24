#include "heterosched/trace_exporter.hpp"
#include <fstream>

namespace heterosched {

nlohmann::json TraceExporter::export_chrome_trace(const TaskGraph& graph, std::size_t cpu_slots, std::size_t gpu_slots, std::size_t dma_engines) {
    nlohmann::json trace_events = nlohmann::json::array();

    // 1. Process / Thread name metadata events
    auto add_meta = [&](const std::string& process_name, const std::string& thread_name, int pid, int tid) {
        trace_events.push_back({
            {"name", "process_name"},
            {"ph", "M"},
            {"pid", pid},
            {"args", {{"name", process_name}}}
        });
        trace_events.push_back({
            {"name", "thread_name"},
            {"ph", "M"},
            {"pid", pid},
            {"tid", tid},
            {"args", {{"name", thread_name}}}
        });
    };

    // PID 1: CPU Workers
    for (std::size_t s = 0; s < cpu_slots; ++s) {
        add_meta("CPU", "Worker " + std::to_string(s), 1, static_cast<int>(s));
    }

    // PID 2: GPU Streams
    for (std::size_t s = 0; s < gpu_slots; ++s) {
        add_meta("GPU", "Stream " + std::to_string(s), 2, static_cast<int>(s));
    }

    // PID 3: PCIe Interconnect DMA Engines
    for (std::size_t e = 0; e < dma_engines; ++e) {
        add_meta("PCIe Interconnect", "DMA Engine " + std::to_string(e), 3, static_cast<int>(e));
    }

    // 2. Task Duration Events
    for (const auto& [id, task] : graph.get_tasks()) {
        if (task.state != TaskState::COMPLETED) continue;

        // A. If task had a DMA transfer
        if (task.transfer_end_tick > task.transfer_start_tick) {
            nlohmann::json dma_ev;
            dma_ev["name"] = "DMA_H2D: " + task.name;
            dma_ev["cat"] = "transfer";
            dma_ev["ph"] = "X";
            dma_ev["ts"] = task.transfer_start_tick;
            dma_ev["dur"] = task.transfer_end_tick - task.transfer_start_tick;
            dma_ev["pid"] = 3; // PCIe
            dma_ev["tid"] = static_cast<int>(task.assigned_dma.value);
            dma_ev["args"] = {
                {"task_id", task.id.value},
                {"bytes", task.input_bytes}
            };
            trace_events.push_back(std::move(dma_ev));
        }

        // B. Compute Kernel execution event
        if (task.finish_tick > task.compute_start_tick) {
            bool is_gpu = (task.assigned_device.value == 1);
            nlohmann::json comp_ev;
            comp_ev["name"] = (is_gpu ? "GPU_Kernel: " : "CPU_Task: ") + task.name;
            comp_ev["cat"] = "compute";
            comp_ev["ph"] = "X";
            comp_ev["ts"] = task.compute_start_tick;
            comp_ev["dur"] = task.finish_tick - task.compute_start_tick;
            comp_ev["pid"] = is_gpu ? 2 : 1;
            comp_ev["tid"] = static_cast<int>(task.assigned_slot.value);
            comp_ev["args"] = {
                {"task_id", task.id.value},
                {"device", is_gpu ? "GPU" : "CPU"},
                {"working_bytes", task.working_bytes},
                {"output_bytes", task.output_bytes}
            };
            trace_events.push_back(std::move(comp_ev));
        }
    }

    nlohmann::json root;
    root["traceEvents"] = trace_events;
    root["displayTimeUnit"] = "ms";
    return root;
}

bool TraceExporter::save_trace_file(const std::string& file_path, const TaskGraph& graph,
                                   std::size_t cpu_slots, std::size_t gpu_slots, std::size_t dma_engines) {
    auto j = export_chrome_trace(graph, cpu_slots, gpu_slots, dma_engines);
    std::ofstream out(file_path);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return true;
}

} // namespace heterosched
