#include "catch_amalgamated.hpp"
#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/event_engine.hpp"
#include "heterosched/policy_factory.hpp"

using namespace heterosched;

TEST_CASE("Hand-Calculated Golden Diamond DAG produces exact analytical schedule ticks", "[golden][fixtures]") {
    // 1. Build Analytical Diamond DAG
    TaskGraph graph;

    // Task 1: Root task (runs on CPU)
    Task t1;
    t1.id = TaskId{1};
    t1.name = "Root_T1";
    t1.arrival_tick = 0;
    t1.eligibility = DeviceEligibility::CPU_ONLY;
    t1.cpu_compute_duration = 100;
    t1.output_bytes = 10000;
    graph.add_task(t1);

    // Task 2: GPU branch (needs H2D transfer of 10,000 bytes)
    Task t2;
    t2.id = TaskId{2};
    t2.name = "GPU_Branch_T2";
    t2.arrival_tick = 0;
    t2.eligibility = DeviceEligibility::GPU_ONLY;
    t2.gpu_compute_duration = 50;
    t2.input_bytes = 10000;
    t2.output_bytes = 0;
    graph.add_task(t2);

    // Task 3: CPU branch
    Task t3;
    t3.id = TaskId{3};
    t3.name = "CPU_Branch_T3";
    t3.arrival_tick = 0;
    t3.eligibility = DeviceEligibility::CPU_ONLY;
    t3.cpu_compute_duration = 80;
    t3.output_bytes = 0;
    graph.add_task(t3);

    // Task 4: Sink task dependent on both T2 and T3
    Task t4;
    t4.id = TaskId{4};
    t4.name = "Sink_T4";
    t4.arrival_tick = 0;
    t4.eligibility = DeviceEligibility::CPU_ONLY;
    t4.cpu_compute_duration = 40;
    graph.add_task(t4);

    // Dependencies: 1 -> 2, 1 -> 3, 2 -> 4, 3 -> 4
    REQUIRE(graph.add_dependency(TaskId{1}, TaskId{2}));
    REQUIRE(graph.add_dependency(TaskId{1}, TaskId{3}));
    REQUIRE(graph.add_dependency(TaskId{2}, TaskId{4}));
    REQUIRE(graph.add_dependency(TaskId{3}, TaskId{4}));

    // Simulator Configuration: 1 CPU slot, 1 GPU slot, 1 DMA engine, Bandwidth = 1,000 bytes/us, Setup = 0 us
    SimulatorConfig config;
    config.cpu_slots = 1;
    config.gpu_slots = 1;
    config.dma_engines = 1;
    config.pcie_bandwidth_bytes_per_tick = 1000;
    config.pcie_setup_latency_ticks = 0;

    auto policy = create_eft_placement_policy();
    EventEngine engine(graph, config, std::move(policy));

    // Execute simulation
    engine.run();

    const auto& res_graph = engine.get_graph();
    const auto& metrics = engine.get_metrics().get_metrics();

    // Verify Task 1 exact analytical ticks: [0, 100] on CPU
    const auto* res_t1 = res_graph.get_task(TaskId{1});
    REQUIRE(res_t1 != nullptr);
    REQUIRE(res_t1->state == TaskState::COMPLETED);
    CHECK(res_t1->compute_start_tick == 0);
    CHECK(res_t1->finish_tick == 100);

    // Verify Task 2 exact analytical ticks: DMA [100, 110], Compute [110, 160] on GPU
    const auto* res_t2 = res_graph.get_task(TaskId{2});
    REQUIRE(res_t2 != nullptr);
    REQUIRE(res_t2->state == TaskState::COMPLETED);
    CHECK(res_t2->transfer_start_tick == 100);
    CHECK(res_t2->transfer_end_tick == 110);
    CHECK(res_t2->compute_start_tick == 110);
    CHECK(res_t2->finish_tick == 160);

    // Verify Task 3 exact analytical ticks: Compute [100, 180] on CPU
    const auto* res_t3 = res_graph.get_task(TaskId{3});
    REQUIRE(res_t3 != nullptr);
    REQUIRE(res_t3->state == TaskState::COMPLETED);
    CHECK(res_t3->compute_start_tick == 100);
    CHECK(res_t3->finish_tick == 180);

    // Verify Task 4 exact analytical ticks: Compute [180, 220] on CPU (waited for T3 to finish at 180)
    const auto* res_t4 = res_graph.get_task(TaskId{4});
    REQUIRE(res_t4 != nullptr);
    REQUIRE(res_t4->state == TaskState::COMPLETED);
    CHECK(res_t4->compute_start_tick == 180);
    CHECK(res_t4->finish_tick == 220);

    // Total Makespan must be exactly 220 us!
    CHECK(metrics.makespan_ticks == 220);
    CHECK(metrics.completed_tasks == 4);
    CHECK(metrics.infeasible_tasks == 0);
}
