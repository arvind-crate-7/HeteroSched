#include "catch_amalgamated.hpp"
#include "heterosched/types.hpp"
#include "heterosched/task_graph.hpp"
#include "heterosched/event_engine.hpp"
#include "heterosched/policy_factory.hpp"

using namespace heterosched;

TEST_CASE("Adversarial Scenarios: Deadlocks, Infeasibility, and Deadline Salvage", "[adversarial]") {

    SECTION("Permanent Infeasibility: Task exceeding total physical VRAM is marked INFEASIBLE") {
        TaskGraph graph;
        Task t1;
        t1.id = TaskId{1};
        t1.eligibility = DeviceEligibility::GPU_ONLY;
        t1.input_bytes = 100ULL * 1024 * 1024 * 1024; // 100 GB (exceeds 16 GB VRAM)
        graph.add_task(t1);

        SimulatorConfig config;
        config.gpu_vram_bytes = 16ULL * 1024 * 1024 * 1024; // 16 GB
        EventEngine engine(graph, config, create_gpu_only_policy());
        engine.run();

        const auto* res_t1 = engine.get_graph().get_task(TaskId{1});
        REQUIRE(res_t1 != nullptr);
        CHECK(res_t1->state == TaskState::INFEASIBLE);
        CHECK(engine.get_metrics().get_metrics().infeasible_tasks == 1);
        CHECK(engine.get_metrics().get_metrics().completed_tasks == 0);
    }

    SECTION("Deadlock Detection: Halts cleanly when no progress can ever be made") {
        TaskGraph graph;
        Task t1;
        t1.id = TaskId{1};
        t1.eligibility = DeviceEligibility::GPU_ONLY;
        t1.input_bytes = 2000000;
        graph.add_task(t1);

        // Configure GPU with only 1,000,000 bytes (cannot admit 2,000,000)
        SimulatorConfig config;
        config.gpu_vram_bytes = 1000000;

        EventEngine engine(graph, config, create_gpu_only_policy());
        engine.run();

        // Simulator must not hang; it must terminate and detect deadlock or infeasibility
        CHECK((engine.is_deadlocked() || engine.get_metrics().get_metrics().infeasible_tasks > 0));
    }

    SECTION("Deadline Salvage: Task offloaded to GPU to meet deadline") {
        TaskGraph graph;
        Task t1;
        t1.id = TaskId{1};
        t1.arrival_tick = 0;
        t1.eligibility = DeviceEligibility::CPU_OR_GPU;
        t1.cpu_compute_duration = 200; // CPU takes 200 us
        t1.gpu_compute_duration = 30;  // GPU takes 30 us
        t1.input_bytes = 10000;        // DMA takes 10 us
        t1.deadline = 80;              // Deadline at tick 80 (CPU will miss at 200; GPU will finish at 10+30=40, meeting deadline!)
        graph.add_task(t1);

        SimulatorConfig config;
        config.cpu_slots = 1;
        config.gpu_slots = 1;
        config.dma_engines = 1;
        config.pcie_bandwidth_bytes_per_tick = 1000;
        config.pcie_setup_latency_ticks = 0;

        EventEngine engine(graph, config, create_deadline_aware_policy());
        engine.run();

        const auto* res_t1 = engine.get_graph().get_task(TaskId{1});
        REQUIRE(res_t1 != nullptr);
        CHECK(res_t1->state == TaskState::COMPLETED);
        CHECK(res_t1->assigned_device.value == 1); // Dispatched to GPU!
        CHECK(res_t1->finish_tick <= *t1.deadline); // Met deadline!
        CHECK(engine.get_metrics().get_metrics().deadline_miss_count == 0);
    }
}
