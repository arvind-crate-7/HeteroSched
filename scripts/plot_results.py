#!/usr/bin/env python3
import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

def main():
    root_dir = Path(__file__).resolve().parent.parent
    results_dir = root_dir / "results"
    csv_path = results_dir / "benchmark_summary.csv"
    plots_dir = results_dir / "plots"
    plots_dir.mkdir(parents=True, exist_ok=True)

    if not csv_path.exists():
        print(f"Error: {csv_path} not found. Run run_benchmarks.py first.")
        return 1

    df = pd.read_csv(csv_path)

    # Filter out 0 makespan (infeasible runs) for meaningful timing plots
    valid_df = df[df["makespan_ticks"] > 0].copy()

    # Style
    plt.style.use("seaborn-v0_8-whitegrid" if "seaborn-v0_8-whitegrid" in plt.style.available else "default")
    plt.rcParams["font.sans-serif"] = "DejaVu Sans"
    plt.rcParams["font.size"] = 10

    # 1. Makespan Comparison Plot
    plt.figure(figsize=(10, 6))
    ax = sns.barplot(data=valid_df, x="workload", y="makespan_ticks", hue="policy", palette="Blues_r")
    plt.title("Workload Makespan Comparison across Scheduling Policies [SIMULATED]", fontsize=13, fontweight="bold", pad=12)
    plt.xlabel("Workload Benchmark", fontweight="bold")
    plt.ylabel("Makespan (Simulation Ticks / us)", fontweight="bold")
    plt.xticks(rotation=15)
    plt.legend(title="Policy", bbox_to_anchor=(1.02, 1), loc="upper left")
    plt.tight_layout()
    makespan_fig = plots_dir / "makespan_comparison.png"
    plt.savefig(makespan_fig, dpi=300)
    plt.close()
    print(f"Generated: {makespan_fig}")

    # 2. Throughput Comparison Plot
    plt.figure(figsize=(10, 6))
    ax = sns.barplot(data=valid_df, x="workload", y="throughput_tps", hue="policy", palette="viridis")
    plt.title("Throughput Comparison across Scheduling Policies [SIMULATED]", fontsize=13, fontweight="bold", pad=12)
    plt.xlabel("Workload Benchmark", fontweight="bold")
    plt.ylabel("Throughput (Tasks / Second)", fontweight="bold")
    plt.xticks(rotation=15)
    plt.legend(title="Policy", bbox_to_anchor=(1.02, 1), loc="upper left")
    plt.tight_layout()
    throughput_fig = plots_dir / "throughput_comparison.png"
    plt.savefig(throughput_fig, dpi=300)
    plt.close()
    print(f"Generated: {throughput_fig}")

    # 3. Deadline Miss Ratio Plot
    deadline_df = df[df["workload"].str.contains("deadline", case=False)].copy()
    if not deadline_df.empty:
        plt.figure(figsize=(8, 5))
        sns.barplot(data=deadline_df, x="policy", y="deadline_miss_ratio", palette="Reds_r")
        plt.title("Deadline Miss Ratio on Mixed Deadline Workload [%]", fontsize=13, fontweight="bold", pad=12)
        plt.xlabel("Policy", fontweight="bold")
        plt.ylabel("Deadline Miss Ratio (%)", fontweight="bold")
        plt.ylim(0, 100)
        plt.tight_layout()
        miss_fig = plots_dir / "deadline_misses.png"
        plt.savefig(miss_fig, dpi=300)
        plt.close()
        print(f"Generated: {miss_fig}")

    # 4. Resource Utilization Profile (Compute Heavy)
    comp_df = df[df["workload"] == "compute_heavy"].copy()
    if not comp_df.empty:
        plt.figure(figsize=(9, 5))
        util_melt = comp_df.melt(id_vars=["policy"], value_vars=["cpu_util_pct", "gpu_util_pct", "dma_util_pct"],
                                 var_name="resource", value_name="utilization_pct")
        util_melt["resource"] = util_melt["resource"].replace({
            "cpu_util_pct": "CPU Worker Slots",
            "gpu_util_pct": "GPU Stream Slots",
            "dma_util_pct": "PCIe DMA Engines"
        })
        sns.barplot(data=util_melt, x="policy", y="utilization_pct", hue="resource", palette="Set2")
        plt.title("Hardware Resource Utilization Profile (Compute Heavy Workload)", fontsize=13, fontweight="bold", pad=12)
        plt.xlabel("Policy", fontweight="bold")
        plt.ylabel("Utilization (%)", fontweight="bold")
        plt.ylim(0, 100)
        plt.legend(title="Resource")
        plt.tight_layout()
        util_fig = plots_dir / "utilization_profile.png"
        plt.savefig(util_fig, dpi=300)
        plt.close()
        print(f"Generated: {util_fig}")

    print("\nAll publication-quality plots generated successfully in results/plots/!")
    return 0

if __name__ == "__main__":
    exit(main())
