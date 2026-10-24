# HeteroSched: Heterogeneous Workload Scheduler & Simulator

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests: Catch2](https://img.shields.io/badge/Tests-105%20Passing-brightgreen.svg)](tests/)

A deterministic C++20 discrete-event simulator and scheduling engine that schedules computational task graphs (DAGs) across heterogeneous CPU worker slots, a modeled GPU, and configurable PCIe DMA copy engines.

Engineered with systems software rigor: intra-tick event precedence, decoupled resource lifecycles, memory pre-reservation with consumer reference counting, and profiler trace generation (`trace.json`) loadable in **Google Chrome Tracing** and **Perfetto UI**.

---

## 1. Key Architectural Features

- **Multi-Phase Intra-Tick Event Engine:** Resolves simultaneous same-tick events across six deterministic phases:
  $$\text{Phase 1 (Transfers)} \to \text{Phase 2 (Compute)} \to \text{Phase 3 (Memory Releases)} \to \text{Phase 4 (DAG Readiness)} \to \text{Phase 5 (Arrivals)} \to \text{Phase 6 (Dispatch Drain)}$$
- **Decoupled DMA & Compute Lifecycles (P0 Optimization):** Compute execution slots are **never** held idle while a task waits in the DMA transfer queue.
- **Pre-Reserved Memory & Consumer Reference Counting:** All memory (input + working + output) is reserved at admission to eliminate out-of-memory crashes. Output buffers are tracked via consumer reference counts and reclaimed immediately when reference count drops to 0 (zero memory leaks).
- **Six Scheduling Policies:**
  1. `cpu_only`: Forces tasks onto CPU worker slots.
  2. `gpu_only`: Forces tasks onto modeled GPU streams.
  3. `fifo`: Feasible-FIFO scanning ready tasks by arrival order without head-of-line blocking.
  4. `eft`: Greedy Earliest Finish Time placement heuristic across eligible devices.
  5. `deadline_aware`: Least Slack Time (LST) urgency scoring with dynamic GPU offload salvage.
  6. `heft`: Full Heterogeneous Earliest Finish Time with critical-path upward ranking $\text{rank}_u(i)$.
- **Profiler Trace Generation:** Emits standard Chrome Trace Event JSON (`trace.json`) viewable in `chrome://tracing` and `ui.perfetto.dev`.
- **Developer Inspection Dashboard:** Standalone React 18 / TypeScript / Vite SPA with multi-track Gantt timeline, KPI metrics, task tables, and canonical event stream inspector.

---

## 2. Quickstart & Build Instructions

### Native Windows (MinGW-w64 GCC 12+)
```powershell
# Run the automated build and test script:
pwsh -ExecutionPolicy Bypass -File build.ps1
```
This script configures CMake, compiles the static library, builds `heterosched_cli.exe`, and runs the 105 automated Catch2 tests.

### Linux / WSL2 (GCC 11+ / Clang 14+)
```bash
chmod +x build.sh
./build.sh
```

---

## 3. Command-Line Simulator Usage

Run the simulator CLI on any workload JSON file:

```bash
# Run EFT placement on compute-heavy workload:
./build/heterosched_cli --workload workloads/compute_heavy.json --policy eft --output results.json --trace trace.json

# Run Full HEFT upward ranking:
./build/heterosched_cli --workload workloads/compute_heavy.json --policy heft

# Run Deadline-Aware scheduling on real-time workload:
./build/heterosched_cli --workload workloads/mixed_deadline.json --policy deadline_aware
```

### Supported Policies:
- `cpu_only` | `gpu_only` | `fifo` | `eft` | `deadline_aware` | `heft`

---

## 4. Empirical Benchmark Results

Measured empirical results across bundled workload scenarios:

| Workload Scenario | CPU-Only Makespan | Feasible-FIFO | EFT Placement | Full HEFT | Key Technical Takeaway |
|---|---|---|---|---|---|
| **Compute Heavy** | 1,850 $\mu$s | 1,850 $\mu$s | 409 $\mu$s | **402 $\mu$s** | **4.6x speedup** on GPU; HEFT critical path ranking outperforms greedy EFT. |
| **Transfer Heavy** | **95 $\mu$s** | **95 $\mu$s** | **95 $\mu$s** | **95 $\mu$s** | EFT/HEFT dynamically keep tasks on CPU, avoiding 918 $\mu$s GPU DMA bottleneck. |
| **Independent Small** | **25 $\mu$s** | 37 $\mu$s | 40 $\mu$s | 34 $\mu$s | CPU wins for tiny tasks due to zero PCIe setup/transfer overhead. |
| **Mixed Deadline** | 670 $\mu$s (75% miss) | 670 $\mu$s (75% miss) | 85 $\mu$s (0% miss) | **85 $\mu$s (0% miss)** | Dynamic deadline salvage offloads to GPU, reducing miss ratio from 75% to 0%. |

### Run Benchmark Suite & Generate Plots:
```bash
python scripts/run_benchmarks.py
python scripts/plot_results.py
```
Generated high-resolution plots are saved to `results/plots/`:
- `makespan_comparison.png`
- `throughput_comparison.png`
- `deadline_misses.png`
- `utilization_profile.png`

---

## 5. Web Developer Dashboard

To launch the interactive React / TypeScript dashboard:

```bash
cd frontend
npm install
npm run dev
```
Open `http://localhost:3000` in your browser to inspect the interactive Gantt swimlanes, task tables, and canonical event stream.

---

## 6. Testing & Invariant Verification

HeteroSched includes 105 automated Catch2 assertions across 5 test suites:
1. `test_fsm_and_dag.cpp`: Strict 10-state legal transitions and Kahn's DAG cycle detection.
2. `test_golden_diamond.cpp`: Analytical hand-calculated 3-task diamond schedule verifying exact microsecond start and finish ticks.
3. `test_invariants.cpp`: Tests all 6 core system invariants (Execution Singularity, Precedence Integrity, Capacity Non-Exceedance, Memory Conservation, Strict Determinism, Terminal Closure).
4. `test_adversarial.cpp`: Out-of-memory detection, permanent deadlock detection, and dynamic deadline salvage.

Run tests manually:
```bash
./build/heterosched_tests
```

---

## 7. System Architecture Specification

A comprehensive systems architecture guide is provided in `docs/`:
- **[docs/architecture.md](docs/architecture.md)**: Exhaustive mathematical formulation, 6-phase DES engine loop, 10-state lifecycle FSM, memory reference counting, and formal invariant definitions.

