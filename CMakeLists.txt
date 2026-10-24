cmake_minimum_required(VERSION 3.20)
project(HeteroSched VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch2
)

# Core Simulator Library
add_library(heterosched_core STATIC
    src/task_graph.cpp
    src/resource_pool.cpp
    src/transfer_engine.cpp
    src/event_engine.cpp
    src/trace_exporter.cpp
    src/metrics.cpp
    src/json_schema.cpp
    src/policies/cpu_only.cpp
    src/policies/gpu_only.cpp
    src/policies/fifo.cpp
    src/policies/eft_placement.cpp
    src/policies/deadline_aware.cpp
    src/policies/heft_policy.cpp
)

target_include_directories(heterosched_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Command-Line Executable
add_executable(heterosched_cli
    src/main.cpp
)
target_link_libraries(heterosched_cli PRIVATE heterosched_core)

# Test Suite Executable
add_executable(heterosched_tests
    third_party/catch2/catch_amalgamated.cpp
    tests/main_test.cpp
    tests/unit/test_fsm_and_dag.cpp
    tests/fixtures/test_golden_diamond.cpp
    tests/invariants/test_invariants.cpp
    tests/adversarial/test_adversarial.cpp
)
target_link_libraries(heterosched_tests PRIVATE heterosched_core)

enable_testing()
add_test(NAME AllTests COMMAND heterosched_tests)
