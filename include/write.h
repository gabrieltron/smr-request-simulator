#ifndef OUTPUT_WRITE_TO_FILE_H
#define OUTPUT_WRITE_TO_FILE_H

#include <iostream>
#include <string>
#include <unordered_map>

#include "graph.h"
#include "graph_cut.h"
#include "partition_scheme.h"

namespace output {

enum GraphFormats {METIS, DOT};
const std::unordered_map<std::string, GraphFormats> string_to_format({
    {"METIS", GraphFormats::METIS},
    {"DOT", GraphFormats::DOT}
});

void write_graph(
    model::Graph& graph, GraphFormats format, std::ostream& output_stream
);
void write_metis_format(model::Graph& graph, std::ostream& output_stream);
void write_dot_format(model::Graph& graph, std::ostream& output_stream);
void write_cut_info(
    model::Graph& graph,
    workload::PartitionScheme& partition_scheme,
    std::ostream& output_stream
);

}

#endif