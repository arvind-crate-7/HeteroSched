#pragma once

#include "heterosched/types.hpp"
#include "heterosched/task.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

namespace heterosched {

enum class ValidationStatus {
    VALID,
    REJECTED_CYCLE,
    REJECTED_INVALID_SCHEMA,
    REJECTED_UNKNOWN_DEPENDENCY,
    REJECTED_SELF_DEPENDENCY
};

struct ValidationResult {
    ValidationStatus status{ValidationStatus::VALID};
    std::string error_message;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return status == ValidationStatus::VALID;
    }
};

class TaskGraph {
public:
    TaskGraph() = default;

    // Task & Dependency Management
    bool add_task(Task task);
    bool add_dependency(TaskId parent_id, TaskId child_id);

    // DAG Validation via Kahn's algorithm
    ValidationResult validate();

    // Accessors
    [[nodiscard]] std::size_t size() const noexcept { return tasks_.size(); }
    [[nodiscard]] bool empty() const noexcept { return tasks_.empty(); }

    [[nodiscard]] const Task* get_task(TaskId id) const noexcept;
    [[nodiscard]] Task* get_task_mut(TaskId id) noexcept;

    [[nodiscard]] const std::unordered_map<TaskId, Task>& get_tasks() const noexcept { return tasks_; }
    [[nodiscard]] std::unordered_map<TaskId, Task>& get_tasks_mut() noexcept { return tasks_; }

    [[nodiscard]] const std::vector<TaskId>& get_topological_order() const noexcept { return topological_order_; }
    [[nodiscard]] std::vector<TaskId> get_roots() const;

private:
    std::unordered_map<TaskId, Task> tasks_;
    std::vector<TaskId> topological_order_;
};

} // namespace heterosched
