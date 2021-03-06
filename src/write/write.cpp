#include "write.h"

namespace output {

void write_graph(model::Graph& graph, GraphFormats format, std::ostream& output_stream) {
    if (format == GraphFormats::METIS) {
        write_metis_format(graph, output_stream);
    } else if (format == GraphFormats::DOT) {
        write_dot_format(graph, output_stream);
    }
}

void write_metis_format(model::Graph& graph, std::ostream& output_stream) {
    output_stream << graph.n_vertex() << " " << graph.n_edges()/2 << " 011";
    for (auto i = 0; i < graph.n_vertex(); i++) {
        output_stream << "\n" << graph.vertice_weight(i);
        for (auto kv : graph.vertice_edges(i)) {
            auto neighbour = kv.first;
            auto edge_weight = kv.second;
            output_stream << " " << neighbour;
            output_stream << " " << edge_weight;
        }
    }
}

void write_dot_format(model::Graph& graph, std::ostream& output_stream) {
    output_stream << "graph {\n";
    auto in_file = std::unordered_map<int, std::unordered_set<int>>();
    for (auto kv : graph.vertex()) {
        auto vertice = kv.first;
        auto weight = kv.second;
        in_file[vertice] = std::unordered_set<int>();

        std::stringstream id_stream;
        id_stream << "\"id:" << vertice << " w:" << weight << "\"";
        auto id = id_stream.str();
        output_stream << "    " << id << ";\n";

        for (auto kv_2 : graph.vertice_edges(vertice)) {
            auto neighbour = kv_2.first;
            auto edge_weight = kv_2.second;
            if (in_file[neighbour].find(vertice) != in_file[neighbour].end()) {
                continue;
            }

            std::stringstream neighbour_id_stream;
            neighbour_id_stream << "\"id:" << neighbour;
            neighbour_id_stream << " w:" << graph.vertice_weight(neighbour);
            neighbour_id_stream << "\"";
            auto neighbour_id = neighbour_id_stream.str();
            output_stream << "    " << id << " -- ";
            output_stream << neighbour_id;
            output_stream << "[label=\"" << edge_weight << "\"];\n";

            in_file[vertice].insert(neighbour);
        }
    }

    output_stream << "}";
}

void write_cut_info(
    model::Graph& graph,
    workload::PartitionManager& partition_scheme,
    std::ostream& output_stream
) {
    output_stream << "Partição | Peso Vértices | Vértices\n";
    auto i = 0;
    for (auto partition : partition_scheme.partitions()) {
        output_stream << i << " | " << partition.weight();
        output_stream << " | ";
        for (auto data : partition.data()) {
            output_stream << " " << data;
        }
        output_stream << "\n";
        i++;
    }
}

void write_log_info(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    write_makespan(execution_log, output_stream);
    write_requests_executed_per_partition(execution_log, output_stream);

    output_stream << "\n";

    write_execution_and_idle_time_per_thread(execution_log, output_stream);
    write_idle_time(execution_log, output_stream);
    write_syncronization_info(execution_log, output_stream);
    output_stream << "\n";
    if (not execution_log.cut_values().empty()) {
        write_cut_info(execution_log, output_stream);
        output_stream << "\n";
    }
    write_busy_threads_per_time(execution_log, output_stream);
    output_stream << "\n";
}

void write_makespan(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    auto makespan = execution_log.makespan();
    output_stream << "Makespan: " << makespan << "\n";
}

void write_requests_executed_per_partition(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    auto n_requests = execution_log.processed_requests();
    auto requests_per_partition = execution_log.requests_per_thread();
    output_stream << "Requests executed: " << n_requests << "\n";
    for (auto i = 0; i < requests_per_partition.size(); i ++) {
        auto n_requests = requests_per_partition[i];

        output_stream << "Partition " << i << ": " << n_requests << "\n";
    }
}

void write_execution_and_idle_time_per_thread(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    output_stream << "Execution time | Idle Time | Idle percentage\n";
    auto makespan = execution_log.makespan();
    auto execution_time = execution_log.execution_time();
    auto idle_time = execution_log.idle_time_per_thread();
    for (auto i = 0; i < execution_time.size(); i++) {
        output_stream << i << ": " << execution_time[i] << " | ";
        output_stream << idle_time[i] << " | ";
        auto idle_percentage = idle_time[i] / double(makespan) * 100.0;
        output_stream << idle_percentage << "%\n";
    }
}

void write_idle_time(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    auto average_idle_time =
        (double) execution_log.idle_time() / execution_log.n_threads();
    output_stream << "Average idle time: " << average_idle_time << "\n";
    output_stream << "\n";
}

void write_syncronization_info(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    output_stream << "Required syncs: " << execution_log.n_syncs() << "\n";
    output_stream << "Crossborder requests executed:\n";
    auto& crossborder_requests = execution_log.crossborder_requests();
    for (auto i = 1; i <= crossborder_requests.size(); i++) {
        output_stream << i << " partitions: ";
        output_stream << crossborder_requests.at(i) << "\n";
    }
}

void write_busy_threads_per_time(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    // Get status
    auto threads_status_per_time =
        execution_log.threads_execution_status_per_time();

    // Fill threads that stayed idle until makespan
    auto makespan = execution_log.makespan();
    for (auto& thread_status_per_time: threads_status_per_time) {
        while (thread_status_per_time.size() < makespan) {
            thread_status_per_time.push_back('\0');
        }
    }

    // Calculate how many threads where actually working and write it
    output_stream << "Busy threads per time: ";
    for (auto i = 0; i < makespan; i++) {
        auto busy_threads = 0;
        for (auto& thread_status_per_time: threads_status_per_time) {
            if (thread_status_per_time.at(i) == '1') {
                busy_threads++;
            }
        }
        output_stream << busy_threads << " ";
    }
    output_stream << "\n";
}

void write_data_partitions(
    std::unordered_map<int, int> data_partitions,
    std::ostream& output_stream
) {
    output_stream << "data_partitions = [ ";
    for (auto kv : data_partitions) {
        auto partition = kv.second;
        output_stream << partition << ", ";
    }
    output_stream << "]";
}

void write_cut_info(
    const workload::ExecutionLog& execution_log,
    std::ostream& output_stream
) {
    output_stream << "Cut values: ";
    for (auto cut_value: execution_log.cut_values()) {
        output_stream << cut_value << " ";
    }
    output_stream << "\n";

    output_stream << "Unbalance values: ";
    for (auto unbalance_value: execution_log.unbalance_values()) {
        output_stream << unbalance_value << " ";
    }
    output_stream << "\n";
}

void write_spanning_tree(
    const model::SpanningTree& tree,
    std::ostream& output_stream
) {
    for (auto& kv : tree.ids_in_nodes()) {
        auto node_id = kv.first;
        auto& ids = kv.second;

        output_stream << "Supernode " << node_id << "\n";
        output_stream << "Weight: " << tree.vertice_weight(node_id) << " ";
        output_stream << "Parent: " << tree.parent(node_id) << "\n";
        output_stream << "Ids: ";
        for (auto id : ids) {
            output_stream << id << " ";
        }
        output_stream << "\n\n";
    }
}

}
