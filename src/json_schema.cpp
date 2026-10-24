#include "heterosched/json_schema.hpp"
#include <fstream>
#include <sstream>

namespace heterosched {

bool JsonSchema::parse_workload_string(const std::string& json_str, TaskGraph& out_graph, SimulatorConfig& out_config, std::string& out_error) {
    try {
        auto j = nlohmann::json::parse(json_str);

        // Parse optional simulator configuration
        if (j.contains("config") && j["config"].is_object()) {
            const auto& cfg = j["config"];
            if (cfg.contains("cpu_slots")) out_config.cpu_slots = cfg["cpu_slots"].get<std::size_t>();
            if (cfg.contains("gpu_slots")) out_config.gpu_slots = cfg["gpu_slots"].get<std::size_t>();
            if (cfg.contains("dma_engines")) out_config.dma_engines = cfg["dma_engines"].get<std::size_t>();
            if (cfg.contains("gpu_vram_bytes")) out_config.gpu_vram_bytes = cfg["gpu_vram_bytes"].get<ByteCount>();
            if (cfg.contains("pcie_bandwidth_bytes_per_tick")) out_config.pcie_bandwidth_bytes_per_tick = cfg["pcie_bandwidth_bytes_per_tick"].get<ByteCount>();
            if (cfg.contains("pcie_setup_latency_ticks")) out_config.pcie_setup_latency_ticks = cfg["pcie_setup_latency_ticks"].get<SimTick>();
            if (cfg.contains("random_seed")) out_config.random_seed = cfg["random_seed"].get<std::uint32_t>();
        }

        // Validate presence of tasks
        if (!j.contains("tasks") || !j["tasks"].is_array()) {
            out_error = "Missing or invalid 'tasks' array in workload schema";
            return false;
        }

        for (const auto& t_json : j["tasks"]) {
            if (!t_json.contains("id")) {
                out_error = "Task missing required 'id' field";
                return false;
            }

            Task task;
            task.id = TaskId{t_json["id"].get<std::uint32_t>()};
            task.name = t_json.value("name", "Task_" + std::to_string(task.id.value));
            task.arrival_tick = t_json.value("arrival_tick", 0ULL);

            std::string elig_str = t_json.value("eligibility", "CPU_OR_GPU");
            if (elig_str == "CPU_ONLY") task.eligibility = DeviceEligibility::CPU_ONLY;
            else if (elig_str == "GPU_ONLY") task.eligibility = DeviceEligibility::GPU_ONLY;
            else task.eligibility = DeviceEligibility::CPU_OR_GPU;

            task.cpu_compute_duration = std::max<SimTick>(1ULL, t_json.value("cpu_compute_duration", 100ULL));
            task.gpu_compute_duration = std::max<SimTick>(1ULL, t_json.value("gpu_compute_duration", 20ULL));

            task.input_bytes = t_json.value("input_bytes", 0ULL);
            task.working_bytes = t_json.value("working_bytes", 0ULL);
            task.output_bytes = t_json.value("output_bytes", 0ULL);

            if (t_json.contains("deadline") && !t_json["deadline"].is_null()) {
                task.deadline = t_json["deadline"].get<SimTick>();
            }

            task.priority = t_json.value("priority", 0U);

            if (t_json.contains("dependencies") && t_json["dependencies"].is_array()) {
                for (const auto& dep : t_json["dependencies"]) {
                    task.dependencies.push_back(TaskId{dep.get<std::uint32_t>()});
                }
            }

            if (!out_graph.add_task(std::move(task))) {
                out_error = "Duplicate task ID: " + std::to_string(t_json["id"].get<std::uint32_t>());
                return false;
            }
        }

        // Cross-link dependencies into successors
        for (const auto& [id, task] : out_graph.get_tasks()) {
            for (const auto& dep_id : task.dependencies) {
                auto* parent = out_graph.get_task_mut(dep_id);
                if (parent) {
                    if (std::find(parent->successors.begin(), parent->successors.end(), id) == parent->successors.end()) {
                        parent->successors.push_back(id);
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        out_error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool JsonSchema::parse_workload_file(const std::string& file_path, TaskGraph& out_graph, SimulatorConfig& out_config, std::string& out_error) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        out_error = "Cannot open workload file: " + file_path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_workload_string(buffer.str(), out_graph, out_config, out_error);
}

nlohmann::json JsonSchema::to_json(const Task& task) {
    nlohmann::json j;
    j["id"] = task.id.value;
    j["name"] = task.name;
    j["state"] = to_string(task.state);
    j["assigned_device"] = (task.state == TaskState::COMPLETED || task.state == TaskState::RUNNING || task.state == TaskState::READY_COMPUTE || task.state == TaskState::TRANSFERRING) ? std::string(to_string(task.assigned_device.value == 1 ? DeviceType::GPU : DeviceType::CPU)) : "NONE";
    j["assigned_slot"] = task.assigned_slot.value;
    j["assigned_dma"] = task.assigned_dma.value;
    j["arrival_tick"] = task.arrival_tick;
    j["scheduled_tick"] = task.scheduled_tick;
    j["transfer_start_tick"] = task.transfer_start_tick;
    j["transfer_end_tick"] = task.transfer_end_tick;
    j["compute_start_tick"] = task.compute_start_tick;
    j["finish_tick"] = task.finish_tick;
    j["input_bytes"] = task.input_bytes;
    j["working_bytes"] = task.working_bytes;
    j["output_bytes"] = task.output_bytes;
    if (task.deadline.has_value()) {
        j["deadline"] = *task.deadline;
    } else {
        j["deadline"] = nullptr;
    }
    j["priority"] = task.priority;
    j["blocked_reason"] = task.blocked_reason;

    std::vector<std::uint32_t> deps;
    for (const auto& d : task.dependencies) deps.push_back(d.value);
    j["dependencies"] = deps;

    return j;
}

} // namespace heterosched
