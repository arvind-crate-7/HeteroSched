#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/json_schema.hpp"
#include "heterosched/event_engine.hpp"
#include "heterosched/policy_factory.hpp"
#include "heterosched/trace_exporter.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --workload <path>   Path to workload JSON file (required)\n"
              << "  --policy <name>     Scheduling policy [cpu_only|gpu_only|fifo|eft|deadline_aware|heft] (default: eft)\n"
              << "  --output <path>     Path to output results.json (default: results.json)\n"
              << "  --trace <path>      Path to output Chrome trace.json (default: trace.json)\n"
              << "  --csv <path>        Path to output metrics.csv (optional)\n"
              << "  --help              Display this help message\n";
}

int main(int argc, char* argv[]) {
    std::string workload_path;
    std::string policy_name = "eft";
    std::string output_path = "results.json";
    std::string trace_path = "trace.json";
    std::string csv_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--workload" && i + 1 < argc) {
            workload_path = argv[++i];
        } else if (arg == "--policy" && i + 1 < argc) {
            policy_name = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (workload_path.empty()) {
        std::cerr << "Error: --workload <path> is required.\n";
        print_usage(argv[0]);
        return 1;
    }

    auto policy = heterosched::create_policy_by_name(policy_name);
    if (!policy) {
        std::cerr << "Error: Unknown policy name: " << policy_name << "\n";
        return 1;
    }

    heterosched::TaskGraph graph;
    heterosched::SimulatorConfig config;
    std::string parse_err;

    if (!heterosched::JsonSchema::parse_workload_file(workload_path, graph, config, parse_err)) {
        std::cerr << "Failed to parse workload: " << parse_err << "\n";
        return 1;
    }

    std::cout << "============================================================\n"
              << " HeteroSched: Heterogeneous Workload Scheduler Simulator\n"
              << "============================================================\n"
              << " Loaded Workload : " << workload_path << " (" << graph.size() << " tasks)\n"
              << " Policy          : " << policy->name() << "\n"
              << " Resources       : " << config.cpu_slots << " CPU slots | " << config.gpu_slots << " GPU slots | "
              << config.dma_engines << " DMA engine(s)\n"
              << " GPU VRAM        : " << (config.gpu_vram_bytes / (1024 * 1024)) << " MB\n"
              << "------------------------------------------------------------\n"
              << " Executing discrete-event simulation...\n";

    heterosched::EventEngine engine(graph, config, std::move(policy));
    engine.run();

    const auto& metrics_agg = engine.get_metrics();
    const auto& m = metrics_agg.get_metrics();
    const auto& final_graph = engine.get_graph();

    std::cout << " Simulation finished at tick: " << engine.get_current_tick() << " us\n"
              << "------------------------------------------------------------\n"
              << " RESULTS & KEY PERFORMANCE INDICATORS [SIMULATED]\n"
              << "------------------------------------------------------------\n"
              << " Makespan           : " << m.makespan_ticks << " us\n"
              << " Throughput         : " << std::fixed << std::setprecision(2) << m.throughput_tps << " tasks/s\n"
              << " Latency (Mean)     : " << std::fixed << std::setprecision(1) << m.latency_mean_ticks << " us (std: " << m.latency_sample_std_ticks << " us)\n"
              << " Latency Percentiles: p50: " << m.latency_p50_ticks << " us | p95: " << m.latency_p95_ticks << " us | p99: " << m.latency_p99_ticks << " us\n"
              << " Completed Tasks    : " << m.completed_tasks << " / " << m.total_tasks << "\n"
              << " Infeasible Tasks   : " << m.infeasible_tasks << "\n"
              << " Deadline Miss Ratio: " << std::fixed << std::setprecision(2) << (m.deadline_miss_ratio * 100.0) << "% (" << m.deadline_miss_count << " misses)\n"
              << " CPU Slot Util.     : " << std::fixed << std::setprecision(2) << m.cpu_utilization_pct << "%\n"
              << " GPU Stream Util.   : " << std::fixed << std::setprecision(2) << m.gpu_utilization_pct << "%\n"
              << " DMA Engine Util.   : " << std::fixed << std::setprecision(2) << m.dma_utilization_pct << "%\n"
              << " Peak GPU VRAM      : " << (m.peak_vram_bytes / (1024 * 1024)) << " MB\n"
              << " Scheduler Runtime  : " << m.scheduler_wall_clock_us << " us (Host Wall-Clock)\n"
              << "============================================================\n";

    // 1. Export results.json
    nlohmann::json results_json;
    results_json["schema_version"] = "1.0.0";
    results_json["workload_file"] = workload_path;
    results_json["policy"] = policy_name;
    results_json["config"] = {
        {"cpu_slots", config.cpu_slots},
        {"gpu_slots", config.gpu_slots},
        {"dma_engines", config.dma_engines},
        {"gpu_vram_bytes", config.gpu_vram_bytes},
        {"pcie_bandwidth_bytes_per_tick", config.pcie_bandwidth_bytes_per_tick},
        {"pcie_setup_latency_ticks", config.pcie_setup_latency_ticks}
    };
    results_json["metrics"] = metrics_agg.to_json();

    nlohmann::json tasks_arr = nlohmann::json::array();
    for (const auto& [id, task] : final_graph.get_tasks()) {
        tasks_arr.push_back(heterosched::JsonSchema::to_json(task));
    }
    results_json["tasks"] = tasks_arr;

    std::ofstream out_file(output_path);
    if (out_file.is_open()) {
        out_file << results_json.dump(2);
        std::cout << " Saved results JSON to: " << output_path << "\n";
    }

    // 2. Export trace.json for Chrome/Perfetto
    if (heterosched::TraceExporter::save_trace_file(trace_path, final_graph, config.cpu_slots, config.gpu_slots, config.dma_engines)) {
        std::cout << " Saved Chrome Trace JSON to: " << trace_path << " (load in chrome://tracing or ui.perfetto.dev)\n";
    }

    // 3. Export optional CSV
    if (!csv_path.empty()) {
        std::ofstream csv_file(csv_path);
        if (csv_file.is_open()) {
            csv_file << metrics_agg.to_csv();
            std::cout << " Saved metrics CSV to: " << csv_path << "\n";
        }
    }

    return 0;
}
