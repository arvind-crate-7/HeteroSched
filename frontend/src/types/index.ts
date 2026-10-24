export interface TaskRecord {
  id: number;
  name: string;
  state: string;
  assigned_device: string;
  assigned_slot: number;
  assigned_dma: number;
  arrival_tick: number;
  scheduled_tick: number;
  transfer_start_tick: number;
  transfer_end_tick: number;
  compute_start_tick: number;
  finish_tick: number;
  input_bytes: number;
  working_bytes: number;
  output_bytes: number;
  deadline: number | null;
  priority: number;
  blocked_reason: string;
  dependencies: number[];
}

export interface CanonicalEvent {
  tick: number;
  phase: number;
  type: string;
  task_id: number;
  device: string;
  track_id: number;
  metadata: string;
}

export interface MetricsSummary {
  makespan_ticks: number;
  throughput_tps: number;
  latency_mean_ticks: number;
  latency_sample_std_ticks: number;
  latency_p50_ticks: number;
  latency_p95_ticks: number;
  latency_p99_ticks: number;
  deadline_miss_ratio: number;
  deadline_miss_count: number;
  total_tasks: number;
  completed_tasks: number;
  infeasible_tasks: number;
  cpu_utilization_pct: number;
  gpu_utilization_pct: number;
  dma_utilization_pct: number;
  peak_vram_bytes: number;
  scheduler_wall_clock_us: number;
  canonical_events: CanonicalEvent[];
}

export interface SimulationConfig {
  cpu_slots: number;
  gpu_slots: number;
  dma_engines: number;
  gpu_vram_bytes: number;
  pcie_bandwidth_bytes_per_tick: number;
  pcie_setup_latency_ticks: number;
}

export interface SimulationResult {
  schema_version: string;
  workload_file: string;
  policy: string;
  config: SimulationConfig;
  metrics: MetricsSummary;
  tasks: TaskRecord[];
}

export interface BenchmarkRecord {
  workload: string;
  policy: string;
  makespan_ticks: number;
  throughput_tps: number;
  latency_p50: number;
  latency_p95: number;
  completed_tasks: number;
  infeasible_tasks: number;
  deadline_miss_ratio: number;
  cpu_util_pct: number;
  gpu_util_pct: number;
  dma_util_pct: number;
  peak_vram_mb: number;
  runtime_us: number;
}
