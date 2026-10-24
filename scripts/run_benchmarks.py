#!/usr/bin/env python3
import os
import subprocess
import json
import csv
from pathlib import Path

def main():
    root_dir = Path(__file__).resolve().parent.parent
    cli_exe = root_dir / "build" / ("heterosched_cli.exe" if os.name == "nt" else "heterosched_cli")

    if not cli_exe.exists():
        print(f"Error: Executable not found at {cli_exe}. Please build the project first.")
        return 1

    workloads_dir = root_dir / "workloads"
    results_dir = root_dir / "results"
    results_dir.mkdir(parents=True, exist_ok=True)
    traces_dir = results_dir / "traces"
    traces_dir.mkdir(parents=True, exist_ok=True)

    policies = ["cpu_only", "gpu_only", "fifo", "eft", "deadline_aware", "heft"]
    workload_files = sorted(workloads_dir.glob("*.json"))

    benchmark_records = []

    print("============================================================")
    print(" Running HeteroSched Comprehensive Benchmark Suite")
    print("============================================================")

    for wf in workload_files:
        workload_name = wf.stem
        print(f"\n--- Workload: {workload_name} ---")

        for pol in policies:
            res_json_path = traces_dir / f"{workload_name}_{pol}_results.json"
            trace_json_path = traces_dir / f"{workload_name}_{pol}_trace.json"

            cmd = [
                str(cli_exe),
                "--workload", str(wf),
                "--policy", pol,
                "--output", str(res_json_path),
                "--trace", str(trace_json_path)
            ]

            proc = subprocess.run(cmd, capture_output=True, text=True)
            if proc.returncode != 0:
                print(f"  [ERROR] {pol}: failed with code {proc.returncode}")
                continue

            if res_json_path.exists():
                with open(res_json_path) as f:
                    data = json.load(f)
                    m = data.get("metrics", {})
                    record = {
                        "workload": workload_name,
                        "policy": pol,
                        "makespan_ticks": m.get("makespan_ticks", 0),
                        "throughput_tps": round(m.get("throughput_tps", 0.0), 2),
                        "latency_p50": m.get("latency_p50_ticks", 0),
                        "latency_p95": m.get("latency_p95_ticks", 0),
                        "completed_tasks": m.get("completed_tasks", 0),
                        "infeasible_tasks": m.get("infeasible_tasks", 0),
                        "deadline_miss_ratio": round(m.get("deadline_miss_ratio", 0.0) * 100.0, 2),
                        "cpu_util_pct": round(m.get("cpu_utilization_pct", 0.0), 2),
                        "gpu_util_pct": round(m.get("gpu_utilization_pct", 0.0), 2),
                        "dma_util_pct": round(m.get("dma_utilization_pct", 0.0), 2),
                        "peak_vram_mb": round(m.get("peak_vram_bytes", 0) / (1024 * 1024), 2),
                        "runtime_us": m.get("scheduler_wall_clock_us", 0)
                    }
                    benchmark_records.append(record)
                    print(f"  [{pol:<14}] Makespan: {record['makespan_ticks']:>6} us | Throughput: {record['throughput_tps']:>8.1f} tps | Miss: {record['deadline_miss_ratio']:>5.1f}%")

    # Save summary JSON
    summary_json_path = results_dir / "benchmark_summary.json"
    with open(summary_json_path, "w") as f:
        json.dump(benchmark_records, f, indent=2)

    # Save summary CSV
    summary_csv_path = results_dir / "benchmark_summary.csv"
    if benchmark_records:
        keys = benchmark_records[0].keys()
        with open(summary_csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=keys)
            writer.writeheader()
            writer.writerows(benchmark_records)

    print(f"\n============================================================")
    print(f" Benchmark completed! Summary saved to:")
    print(f"   - {summary_json_path}")
    print(f"   - {summary_csv_path}")
    print(f"============================================================")
    return 0

if __name__ == "__main__":
    exit(main())
