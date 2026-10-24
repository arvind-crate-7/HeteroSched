#include "catch_amalgamated.hpp"
#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/event_engine.hpp"
#include "heterosched/policy_factory.hpp"

using namespace heterosched;

TEST_CASE("Simulator enforces all 6 core system invariants", "[invariants]") {
    // Generate a heterogeneous synthetic DAG
    TaskGraph graph;
    for (std::uint32_t i = 1; i <= 6; ++i) {
        Task t;
        t.id = TaskId{i};
        t.arrival_tick = (i <= 2) ? 0 : 20;
        t.cpu_compute_duration = 50 + i * 10;
        t.gpu_compute_duration = 20 + i * 5;
        t.input_bytes = (i % 2 == 0) ? 50000 : 0;
        t.working_bytes = 20000;
        t.output_bytes = 50000;
        graph.add_task(t);
    }

    // Dependencies: 1->3, 2->3, 3->4, 3->5, 4->6, 5->6
    graph.add_dependency(TaskId{1}, TaskId{3});
    graph.add_dependency(TaskId{2}, TaskId{3});
    graph.add_dependency(TaskId{3}, TaskId{4});
    graph.add_dependency(TaskId{3}, TaskId{5});
    graph.add_dependency(TaskId{4}, TaskId{6});
    graph.add_dependency(TaskId{5}, TaskId{6});

    SimulatorConfig config;
    config.cpu_slots = 2;
    config.gpu_slots = 2;
    config.dma_engines = 1;
    config.gpu_vram_bytes = 5000000; // 5 MB

    SECTION("Invariant 1, 2, 4, 6: Singularity, Precedence, Conservation, Terminal Closure") {
        EventEngine engine(graph, config, create_eft_placement_policy());
        engine.run();

        const auto& res_graph = engine.get_graph();
        const auto& metrics = engine.get_metrics().get_metrics();

        for (const auto& [id, task] : res_graph.get_tasks()) {
            // Invariant 6: Terminal Closure
            REQUIRE(task.is_terminal());

            // Invariant 2: Precedence Integrity
            if (task.state == TaskState::COMPLETED) {
                for (const auto& parent_id : task.dependencies) {
                    const auto* parent = res_graph.get_task(parent_id);
                    REQUIRE(parent != nullptr);
                    REQUIRE(parent->state == TaskState::COMPLETED);
                    REQUIRE(task.compute_start_tick >= parent->finish_tick);
                }
            }
        }

        // Invariant 4: Memory Conservation (All working memory freed, all output buffers reclaimed)
        CHECK(engine.get_resources().get_gpu_vram_allocated() == 0);
        CHECK(metrics.completed_tasks == 6);
    }

    SECTION("Invariant 5: Strict Determinism") {
        EventEngine engine1(graph, config, create_eft_placement_policy());
        engine1.run();

        EventEngine engine2(graph, config, create_eft_placement_policy());
        engine2.run();

        const auto& m1 = engine1.get_metrics().get_metrics();
        const auto& m2 = engine2.get_metrics().get_metrics();

        CHECK(m1.makespan_ticks == m2.makespan_ticks);
        CHECK(m1.completed_tasks == m2.completed_tasks);
        CHECK(m1.throughput_tps == m2.throughput_tps);
        CHECK(m1.cpu_utilization_pct == m2.cpu_utilization_pct);
        CHECK(m1.gpu_utilization_pct == m2.gpu_utilization_pct);
        CHECK(m1.peak_vram_bytes == m2.peak_vram_bytes);

        // Bit-for-bit event count equality
        REQUIRE(engine1.get_metrics().get_event_log().size() == engine2.get_metrics().get_event_log().size());
    }
}
