#include "heterosched/task_graph.hpp"
#include <queue>
#include <sstream>

namespace heterosched {

bool TaskGraph::add_task(Task task) {
    if (tasks_.contains(task.id)) {
        return false; // Duplicate task ID
    }
    tasks_.emplace(task.id, std::move(task));
    return true;
}

bool TaskGraph::add_dependency(TaskId parent_id, TaskId child_id) {
    if (parent_id == child_id) return false; // Self dependency

    auto p_it = tasks_.find(parent_id);
    auto c_it = tasks_.find(child_id);
    if (p_it == tasks_.end() || c_it == tasks_.end()) {
        return false; // Unknown task
    }

    // Add child to parent's successors
    auto& successors = p_it->second.successors;
    if (std::find(successors.begin(), successors.end(), child_id) == successors.end()) {
        successors.push_back(child_id);
    }

    // Add parent to child's dependencies
    auto& dependencies = c_it->second.dependencies;
    if (std::find(dependencies.begin(), dependencies.end(), parent_id) == dependencies.end()) {
        dependencies.push_back(parent_id);
    }

    return true;
}

ValidationResult TaskGraph::validate() {
    topological_order_.clear();

    if (tasks_.empty()) {
        return {ValidationStatus::VALID, ""};
    }

    // 1. Basic property validation (positive durations)
    for (auto& [id, task] : tasks_) {
        if (task.cpu_compute_duration == 0 || task.gpu_compute_duration == 0) {
            std::ostringstream oss;
            oss << "Task " << id.value << " has 0 compute duration. Clamping to 1 tick minimum.";
            task.cpu_compute_duration = std::max<SimTick>(1, task.cpu_compute_duration);
            task.gpu_compute_duration = std::max<SimTick>(1, task.gpu_compute_duration);
        }

        // Validate dependency existence
        for (const auto& dep_id : task.dependencies) {
            if (dep_id == id) {
                return {ValidationStatus::REJECTED_SELF_DEPENDENCY,
                        "Self-dependency detected on Task " + std::to_string(id.value)};
            }
            if (!tasks_.contains(dep_id)) {
                return {ValidationStatus::REJECTED_UNKNOWN_DEPENDENCY,
                        "Task " + std::to_string(id.value) + " references unknown dependency " + std::to_string(dep_id.value)};
            }
        }
    }

    // 2. Kahn's Algorithm for Topological Sort & Cycle Detection
    std::unordered_map<TaskId, std::size_t> in_degree;
    for (const auto& [id, task] : tasks_) {
        in_degree[id] = task.dependencies.size();
    }

    std::queue<TaskId> zero_in_degree;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) {
            zero_in_degree.push(id);
        }
    }

    std::size_t visited_count = 0;
    while (!zero_in_degree.empty()) {
        TaskId curr = zero_in_degree.front();
        zero_in_degree.pop();
        topological_order_.push_back(curr);
        visited_count++;

        const Task& task = tasks_.at(curr);
        for (const auto& succ_id : task.successors) {
            in_degree[succ_id]--;
            if (in_degree[succ_id] == 0) {
                zero_in_degree.push(succ_id);
            }
        }
    }

    if (visited_count != tasks_.size()) {
        topological_order_.clear();
        return {ValidationStatus::REJECTED_CYCLE,
                "Cyclic dependency detected in task graph (Kahn's algorithm visited " +
                std::to_string(visited_count) + " of " + std::to_string(tasks_.size()) + " tasks)"};
    }

    // Initialize pending_dependencies for all tasks
    for (auto& [id, task] : tasks_) {
        task.pending_dependencies = task.dependencies.size();
    }

    return {ValidationStatus::VALID, ""};
}

const Task* TaskGraph::get_task(TaskId id) const noexcept {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? &it->second : nullptr;
}

Task* TaskGraph::get_task_mut(TaskId id) noexcept {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? &it->second : nullptr;
}

std::vector<TaskId> TaskGraph::get_roots() const {
    std::vector<TaskId> roots;
    for (const auto& [id, task] : tasks_) {
        if (task.dependencies.empty()) {
            roots.push_back(id);
        }
    }
    return roots;
}

} // namespace heterosched
