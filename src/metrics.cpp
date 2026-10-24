#include "heterosched/metrics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace heterosched {

void MetricsAggregator::record_event(SimTick tick, EventPhase phase, EventType type, TaskId task_id,
                                    DeviceType device, std::uint32_t track_id, std::string metadata) {
    event_log_.push_back(CanonicalEventRecord{
        .tick = tick,
        .phase = phase,
        .type = type,
        .task_id = task_id,
        .device = device,
        .track_id = track_id,
        .metadata = std::move(metadata)
    });
}

void MetricsAggregator::finalize(SimTick /*final_tick*/, const TaskGraph& graph, const ResourcePool& resources,
                                std::size_t dma_engines, SimTick total_dma_busy_ticks,
                                SimTick total_cpu_busy_ticks, SimTick total_gpu_busy_ticks,
                                SimTick wall_clock_us) {
    metrics_.scheduler_wall_clock_us = wall_clock_us;
    metrics_.peak_vram_bytes = resources.get_peak_gpu_vram();
    metrics_.total_tasks = graph.size();

    if (graph.empty()) {
        return;
    }

    SimTick min_arrival = std::numeric_limits<SimTick>::max();
    SimTick max_finish = 0;
    std::vector<SimTick> latencies;
    std::size_t deadline_tasks = 0;
    std::size_t misses = 0;

    for (const auto& [id, task] : graph.get_tasks()) {
        if (task.arrival_tick < min_arrival) {
            min_arrival = task.arrival_tick;
        }

        if (task.state == TaskState::COMPLETED) {
            metrics_.completed_tasks++;
            if (task.finish_tick > max_finish) {
                max_finish = task.finish_tick;
            }
            if (task.finish_tick >= task.arrival_tick) {
                latencies.push_back(task.finish_tick - task.arrival_tick);
            }
            if (task.deadline.has_value()) {
                deadline_tasks++;
                if (task.finish_tick > *task.deadline) {
                    misses++;
                }
            }
        } else if (task.state == TaskState::INFEASIBLE || task.state == TaskState::FAILED_DEPENDENCY) {
            metrics_.infeasible_tasks++;
        }
    }

    metrics_.deadline_miss_count = misses;
    metrics_.deadline_miss_ratio = (deadline_tasks > 0) ? (static_cast<double>(misses) / static_cast<double>(deadline_tasks)) : 0.0;

    if (metrics_.completed_tasks > 0 && max_finish >= min_arrival) {
        metrics_.makespan_ticks = max_finish - min_arrival;
        if (metrics_.makespan_ticks > 0) {
            double makespan_seconds = static_cast<double>(metrics_.makespan_ticks) * 1e-6;
            metrics_.throughput_tps = static_cast<double>(metrics_.completed_tasks) / makespan_seconds;
        }
    } else {
        metrics_.makespan_ticks = 0;
        metrics_.throughput_tps = 0.0;
    }

    // Latency percentiles & sample standard deviation
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        metrics_.latency_mean_ticks = sum / static_cast<double>(latencies.size());

        if (latencies.size() > 1) {
            double variance_sum = 0.0;
            for (auto lat : latencies) {
                double diff = static_cast<double>(lat) - metrics_.latency_mean_ticks;
                variance_sum += diff * diff;
            }
            metrics_.latency_sample_std_ticks = std::sqrt(variance_sum / static_cast<double>(latencies.size() - 1));
        }

        auto get_percentile = [&](double pct) -> SimTick {
            std::size_t idx = static_cast<std::size_t>(std::ceil(pct * static_cast<double>(latencies.size()))) - 1;
            idx = std::min(idx, latencies.size() - 1);
            return latencies[idx];
        };

        metrics_.latency_p50_ticks = get_percentile(0.50);
        metrics_.latency_p95_ticks = get_percentile(0.95);
        metrics_.latency_p99_ticks = get_percentile(0.99);
    }

    // Slot & DMA utilizations across makespan measurement window
    SimTick window = std::max<SimTick>(1, metrics_.makespan_ticks);
    std::size_t cpu_slots = resources.get_max_slots(DeviceType::CPU);
    std::size_t gpu_slots = resources.get_max_slots(DeviceType::GPU);

    metrics_.cpu_utilization_pct = (cpu_slots > 0) ?
        std::min(100.0, (static_cast<double>(total_cpu_busy_ticks) / static_cast<double>(cpu_slots * window)) * 100.0) : 0.0;

    metrics_.gpu_utilization_pct = (gpu_slots > 0) ?
        std::min(100.0, (static_cast<double>(total_gpu_busy_ticks) / static_cast<double>(gpu_slots * window)) * 100.0) : 0.0;

    metrics_.dma_utilization_pct = (dma_engines > 0) ?
        std::min(100.0, (static_cast<double>(total_dma_busy_ticks) / static_cast<double>(dma_engines * window)) * 100.0) : 0.0;
}

nlohmann::json MetricsAggregator::to_json() const {
    nlohmann::json j;
    j["makespan_ticks"] = metrics_.makespan_ticks;
    j["throughput_tps"] = metrics_.throughput_tps;
    j["latency_mean_ticks"] = metrics_.latency_mean_ticks;
    j["latency_sample_std_ticks"] = metrics_.latency_sample_std_ticks;
    j["latency_p50_ticks"] = metrics_.latency_p50_ticks;
    j["latency_p95_ticks"] = metrics_.latency_p95_ticks;
    j["latency_p99_ticks"] = metrics_.latency_p99_ticks;
    j["deadline_miss_ratio"] = metrics_.deadline_miss_ratio;
    j["deadline_miss_count"] = metrics_.deadline_miss_count;
    j["total_tasks"] = metrics_.total_tasks;
    j["completed_tasks"] = metrics_.completed_tasks;
    j["infeasible_tasks"] = metrics_.infeasible_tasks;
    j["cpu_utilization_pct"] = metrics_.cpu_utilization_pct;
    j["gpu_utilization_pct"] = metrics_.gpu_utilization_pct;
    j["dma_utilization_pct"] = metrics_.dma_utilization_pct;
    j["peak_vram_bytes"] = metrics_.peak_vram_bytes;
    j["scheduler_wall_clock_us"] = metrics_.scheduler_wall_clock_us;

    nlohmann::json events_arr = nlohmann::json::array();
    for (const auto& ev : event_log_) {
        events_arr.push_back({
            {"tick", ev.tick},
            {"phase", static_cast<std::uint8_t>(ev.phase)},
            {"type", std::string(to_string(ev.type))},
            {"task_id", ev.task_id.value},
            {"device", std::string(to_string(ev.device))},
            {"track_id", ev.track_id},
            {"metadata", ev.metadata}
        });
    }
    j["canonical_events"] = events_arr;

    return j;
}

std::string MetricsAggregator::to_csv() const {
    std::ostringstream oss;
    oss << "metric,value\n"
        << "makespan_ticks," << metrics_.makespan_ticks << "\n"
        << "throughput_tps," << std::fixed << std::setprecision(2) << metrics_.throughput_tps << "\n"
        << "latency_mean_ticks," << std::fixed << std::setprecision(2) << metrics_.latency_mean_ticks << "\n"
        << "latency_sample_std_ticks," << std::fixed << std::setprecision(2) << metrics_.latency_sample_std_ticks << "\n"
        << "latency_p50_ticks," << metrics_.latency_p50_ticks << "\n"
        << "latency_p95_ticks," << metrics_.latency_p95_ticks << "\n"
        << "latency_p99_ticks," << metrics_.latency_p99_ticks << "\n"
        << "deadline_miss_ratio," << std::fixed << std::setprecision(4) << metrics_.deadline_miss_ratio << "\n"
        << "completed_tasks," << metrics_.completed_tasks << "\n"
        << "infeasible_tasks," << metrics_.infeasible_tasks << "\n"
        << "cpu_utilization_pct," << std::fixed << std::setprecision(2) << metrics_.cpu_utilization_pct << "\n"
        << "gpu_utilization_pct," << std::fixed << std::setprecision(2) << metrics_.gpu_utilization_pct << "\n"
        << "dma_utilization_pct," << std::fixed << std::setprecision(2) << metrics_.dma_utilization_pct << "\n"
        << "peak_vram_bytes," << metrics_.peak_vram_bytes << "\n"
        << "scheduler_wall_clock_us," << metrics_.scheduler_wall_clock_us << "\n";
    return oss.str();
}

} // namespace heterosched
