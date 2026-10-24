#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/resource_pool.hpp"
#include "nlohmann/json.hpp"
#include <vector>
#include <string>

namespace heterosched {

struct CanonicalEventRecord {
    SimTick tick{0};
    EventPhase phase{EventPhase::Phase5_WorkloadArrival};
    EventType type{EventType::WORKLOAD_ARRIVAL};
    TaskId task_id{0};
    DeviceType device{DeviceType::CPU};
    std::uint32_t track_id{0}; // Compute slot ID or DMA Engine ID
    std::string metadata;
};

struct SimulationMetrics {
    SimTick makespan_ticks{0};
    double throughput_tps{0.0}; // Tasks / second

    double latency_mean_ticks{0.0};
    double latency_sample_std_ticks{0.0};
    SimTick latency_p50_ticks{0};
    SimTick latency_p95_ticks{0};
    SimTick latency_p99_ticks{0};

    double deadline_miss_ratio{0.0};
    std::size_t total_tasks{0};
    std::size_t completed_tasks{0};
    std::size_t infeasible_tasks{0};
    std::size_t deadline_miss_count{0};

    double cpu_utilization_pct{0.0};
    double gpu_utilization_pct{0.0};
    double dma_utilization_pct{0.0};
    ByteCount peak_vram_bytes{0};

    SimTick scheduler_wall_clock_us{0};
};

class MetricsAggregator {
public:
    MetricsAggregator() = default;

    void record_event(SimTick tick, EventPhase phase, EventType type, TaskId task_id,
                      DeviceType device, std::uint32_t track_id, std::string metadata = "");

    void finalize(SimTick final_tick, const TaskGraph& graph, const ResourcePool& resources,
                  std::size_t dma_engines, SimTick total_dma_busy_ticks,
                  SimTick total_cpu_busy_ticks, SimTick total_gpu_busy_ticks,
                  SimTick wall_clock_us);

    [[nodiscard]] const SimulationMetrics& get_metrics() const noexcept { return metrics_; }
    [[nodiscard]] const std::vector<CanonicalEventRecord>& get_event_log() const noexcept { return event_log_; }

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] std::string to_csv() const;

private:
    std::vector<CanonicalEventRecord> event_log_;
    SimulationMetrics metrics_;
};

} // namespace heterosched
