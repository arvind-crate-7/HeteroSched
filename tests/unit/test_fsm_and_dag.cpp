#include "catch_amalgamated.hpp"
#include "heterosched/types.hpp"
#include "heterosched/task.hpp"
#include "heterosched/task_graph.hpp"

using namespace heterosched;

TEST_CASE("Task Lifecycle FSM enforces legal state transitions", "[fsm]") {
    Task task;
    task.id = TaskId{1};
    REQUIRE(task.state == TaskState::UNSUBMITTED);

    SECTION("Legal path: UNSUBMITTED -> WAITING_DEPS -> READY -> QUEUED_DMA -> TRANSFERRING -> READY_COMPUTE -> RUNNING -> COMPLETED") {
        REQUIRE_NOTHROW(task.transition_to(TaskState::WAITING_DEPS));
        REQUIRE(task.state == TaskState::WAITING_DEPS);

        REQUIRE_NOTHROW(task.transition_to(TaskState::READY));
        REQUIRE(task.state == TaskState::READY);

        REQUIRE_NOTHROW(task.transition_to(TaskState::QUEUED_DMA));
        REQUIRE(task.state == TaskState::QUEUED_DMA);

        REQUIRE_NOTHROW(task.transition_to(TaskState::TRANSFERRING));
        REQUIRE(task.state == TaskState::TRANSFERRING);

        REQUIRE_NOTHROW(task.transition_to(TaskState::READY_COMPUTE));
        REQUIRE(task.state == TaskState::READY_COMPUTE);

        REQUIRE_NOTHROW(task.transition_to(TaskState::RUNNING));
        REQUIRE(task.state == TaskState::RUNNING);

        REQUIRE_NOTHROW(task.transition_to(TaskState::COMPLETED));
        REQUIRE(task.state == TaskState::COMPLETED);
        REQUIRE(task.is_terminal());
    }

    SECTION("Legal shortcut: READY -> READY_COMPUTE (when data is resident)") {
        task.transition_to(TaskState::READY);
        REQUIRE_NOTHROW(task.transition_to(TaskState::READY_COMPUTE));
        REQUIRE(task.state == TaskState::READY_COMPUTE);
    }

    SECTION("Illegal transitions throw invariant violation") {
        task.transition_to(TaskState::READY);
        // Cannot jump directly from READY to RUNNING without READY_COMPUTE
        REQUIRE_THROWS_AS(task.transition_to(TaskState::RUNNING), std::logic_error);

        // Terminal state cannot transition to anything
        Task comp_task;
        comp_task.state = TaskState::COMPLETED;
        REQUIRE_THROWS_AS(comp_task.transition_to(TaskState::RUNNING), std::logic_error);
    }
}

TEST_CASE("TaskGraph validates DAG acyclicity and detects cycles via Kahn's algorithm", "[dag]") {
    TaskGraph graph;

    SECTION("Valid 3-task linear chain passes") {
        Task t1; t1.id = TaskId{1};
        Task t2; t2.id = TaskId{2};
        Task t3; t3.id = TaskId{3};

        REQUIRE(graph.add_task(t1));
        REQUIRE(graph.add_task(t2));
        REQUIRE(graph.add_task(t3));

        REQUIRE(graph.add_dependency(TaskId{1}, TaskId{2}));
        REQUIRE(graph.add_dependency(TaskId{2}, TaskId{3}));

        auto res = graph.validate();
        REQUIRE(res.is_valid());
        REQUIRE(graph.get_topological_order().size() == 3);
        REQUIRE(graph.get_topological_order()[0] == TaskId{1});
        REQUIRE(graph.get_topological_order()[1] == TaskId{2});
        REQUIRE(graph.get_topological_order()[2] == TaskId{3});
    }

    SECTION("Cyclic graph is rejected with REJECTED_CYCLE") {
        Task t1; t1.id = TaskId{1};
        Task t2; t2.id = TaskId{2};

        graph.add_task(t1);
        graph.add_task(t2);

        graph.add_dependency(TaskId{1}, TaskId{2});
        graph.add_dependency(TaskId{2}, TaskId{1}); // Cycle!

        auto res = graph.validate();
        REQUIRE_FALSE(res.is_valid());
        REQUIRE(res.status == ValidationStatus::REJECTED_CYCLE);
    }

    SECTION("Duplicate task ID is rejected") {
        Task t1; t1.id = TaskId{1};
        Task t1_dup; t1_dup.id = TaskId{1};

        REQUIRE(graph.add_task(t1));
        REQUIRE_FALSE(graph.add_task(t1_dup));
    }

    SECTION("Self dependency is rejected") {
        Task t1; t1.id = TaskId{1};
        graph.add_task(t1);
        REQUIRE_FALSE(graph.add_dependency(TaskId{1}, TaskId{1}));
    }
}
