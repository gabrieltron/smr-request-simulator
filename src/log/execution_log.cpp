#include "execution_log.h"

namespace workload {

ExecutionLog::ExecutionLog(int n_threads) {
    for (auto i = 0; i < n_threads; i++) {
        simulated_threads_[i] = Thread();
        crossborder_requests_[i+1] = 0;
    }
}

void ExecutionLog::increase_elapsed_time(int thread_id, int time/*=1*/) {
    simulated_threads_[thread_id].elapsed_time_ += time;
}

void ExecutionLog::execute_request(int thread_id, int execution_time/*=1*/) {
    simulated_threads_[thread_id].executed_requests_ += 1;
    processed_requests_ += 1;
    for (auto i = 0; i < execution_time; i++) {
        simulated_threads_[thread_id].executing_on_time_.push_back('1');
    }
    increase_elapsed_time(thread_id, execution_time);
}

void ExecutionLog::sync_all_partitions() {
    std::unordered_set<int> involved_threads;
    for (auto i = 0; i < simulated_threads_.size(); i++) {
        involved_threads.insert(i);
    }
    sync_partitions(involved_threads);
}

void ExecutionLog::sync_partitions(
    const std::unordered_set<int>& thread_ids
) {
    auto timeskip = max_elapsed_time(thread_ids);
    for (auto thread : thread_ids) {
        skip_time(thread, timeskip);
    }
    crossborder_requests_[thread_ids.size()]++;
    if (thread_ids.size() > 1) {
        sync_counter_++;
    }
}

void ExecutionLog::skip_time(int thread_id, int value) {
    auto skipped_time = value - simulated_threads_[thread_id].elapsed_time_;
    for (auto i = 0; i < skipped_time; i++) {
        simulated_threads_.at(thread_id).executing_on_time_.push_back('\0');
    }
    simulated_threads_[thread_id].idle_time_ += skipped_time;
    simulated_threads_[thread_id].elapsed_time_ = value;
}

void ExecutionLog::increase_sync_counter() {
    sync_counter_++;
}

int ExecutionLog::partition_with_longest_execution(const std::unordered_set<int>& partitions) const {
    auto max = -1;
    auto partition = 0;
    for (const auto& partition_id: partitions) {
        const auto& thread = simulated_threads_.at(partition_id);
        if (thread.elapsed_time_ > max) {
            max = thread.elapsed_time_;
            partition = partition_id;
        }
    }
    return partition;
}

std::vector<std::vector<char>> ExecutionLog::threads_execution_status_per_time() const {
    std::vector<std::vector<char>> threads_status;
    for (auto i = 0; i < simulated_threads_.size(); i++) {
        threads_status.push_back(simulated_threads_.at(i).executing_on_time_);
    }
    return threads_status;
}


int ExecutionLog::max_elapsed_time(
    const std::unordered_set<int>& thread_ids
) const {
    auto max = 0;
    for (auto id : thread_ids) {
        auto elapsed_time = simulated_threads_.at(id).elapsed_time_;
        if (elapsed_time > max) {
            max = elapsed_time;
        }
    }
    return max;
}

int ExecutionLog::makespan() const {
    auto longest_time = 0;
    for (const auto& kv: simulated_threads_) {
        const auto& thread = kv.second;
        auto time = thread.elapsed_time_;
        if (longest_time < time) {
            longest_time = time;
        }
    }
    return longest_time;
}

int ExecutionLog::n_threads() const {
    return simulated_threads_.size();
}

int ExecutionLog::n_syncs() const {
    return sync_counter_;
}

int ExecutionLog::processed_requests() const {
    return processed_requests_;
}

int ExecutionLog::elapsed_time(int thread_id) const {
    return simulated_threads_.at(thread_id).elapsed_time_;
}

int ExecutionLog::idle_time() const {
    auto total_time = 0;
    for (const auto& kv : simulated_threads_) {
        const auto& thread = kv.second;
        auto time = thread.idle_time_;
        total_time += time + (makespan() - thread.elapsed_time_);
    }
    return total_time;
}

std::unordered_map<int, int> ExecutionLog::idle_time_per_thread() const {
    auto idle_time = std::unordered_map<int, int>();

    for (const auto& kv: simulated_threads_) {
        auto thread_id = kv.first;
        const auto& thread = kv.second;
        idle_time[thread_id] = thread.idle_time_ + makespan() - thread.elapsed_time_;
    }

    return idle_time;
}

std::unordered_map<int, int> ExecutionLog::requests_per_thread() const {
    auto requests = std::unordered_map<int, int>();

    for (const auto& kv: simulated_threads_) {
        auto thread_id = kv.first;
        const auto& thread = kv.second;
        requests[thread_id] = thread.executed_requests_;
    }

    return requests;
}

const std::unordered_map<int, int> ExecutionLog::execution_time() const{
    auto elapsed_time = std::unordered_map<int, int>();
    for (const auto& kv : simulated_threads_) {
        auto thread_id = kv.first;
        const auto& thread = kv.second;
        elapsed_time[thread_id] = thread.elapsed_time_;
    }
    return elapsed_time;
}

const std::unordered_map<int, int>& ExecutionLog::crossborder_requests() const {
    return crossborder_requests_;
}

const std::vector<int>& ExecutionLog::cut_values() const {
    return cut_values_;
}

const std::vector<double>& ExecutionLog::unbalance_values() const {
    return unbalance_values_;
}

void ExecutionLog::register_repartition(const PartitionManager& partition_manager) {
    register_cut_value(partition_manager);
    register_unbalance_value(partition_manager);
}

void ExecutionLog::register_cut_value(const PartitionManager& partition_manager) {
    auto cut_value = 0;
    for (auto kv: partition_manager.value_to_partition_map()) {
        auto value = kv.first;
        auto partition = kv.second;
        const auto& neighbours_and_weight =
            partition_manager.access_graph().vertice_edges(value);
        for (auto neighbours_and_weight: neighbours_and_weight) {
            auto neighbour = neighbours_and_weight.first;
            auto weight = neighbours_and_weight.second;

            if (partition_manager.value_to_partition(neighbour) != partition) {
                cut_value += weight;
            }
        }
    }

    cut_values_.push_back(cut_value);
}

void ExecutionLog::register_unbalance_value(const PartitionManager& partition_manager) {
    auto total_weight = 0;
    for (const auto& partition: partition_manager.partitions()) {
        total_weight += partition.weight();
    }
    auto ideal_partition_weight = (double) total_weight / partition_manager.n_partitions();

    auto biggest_unbalance = 0;
    for (const auto& partition: partition_manager.partitions()) {
        auto partition_weight = partition.weight();
        if (abs(partition_weight - ideal_partition_weight) > biggest_unbalance) {
            biggest_unbalance = partition_weight - ideal_partition_weight;
        }
    }
    auto unbalance_percentage = 100*biggest_unbalance/ideal_partition_weight;
    if (unbalance_percentage < 0) {
        unbalance_percentage *= -1;
    }
    unbalance_values_.push_back(unbalance_percentage);
}


}
