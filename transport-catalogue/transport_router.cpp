#include "transport_router.h"

namespace transport {

constexpr double kDistanceFactor = 100.0;
constexpr double kSpeedFactor = 6.0;

void Router::AddStopEdges(const Catalogue& catalogue, graph::DirectedWeightedGraph<double>& graph, std::map<std::string, graph::VertexId>& stop_ids, graph::VertexId& vertex_id) {
  const auto& all_stops = catalogue.GetSortedAllStops();
  for (const auto& [stop_name, stop_info] : all_stops) {
    stop_ids[stop_info->name] = vertex_id;
    graph.AddEdge({
      stop_info->name,
      0,
      vertex_id,
      ++vertex_id,
      static_cast<double>(bus_wait_time_)
    });
    ++vertex_id;
  }
}

void Router::AddBusEdges(const Catalogue& catalogue, graph::DirectedWeightedGraph<double>& graph, const std::map<std::string, graph::VertexId>& stop_ids) {
  const auto& all_buses = catalogue.GetSortedAllBuses();
  for_each(
    all_buses.begin(),
    all_buses.end(),
    [&graph, this, &catalogue, &stop_ids](const auto& item) {
      const auto& bus_info = item.second;
      const auto& stops = bus_info->stops;
      size_t stops_count = stops.size();
      for (size_t i = 0; i < stops_count; ++i) {
        for (size_t j = i + 1; j < stops_count; ++j) {
          const Stop* stop_from = stops[i];
          const Stop* stop_to = stops[j];
          int dist_sum = 0;
          int dist_sum_inverse = 0;
          for (size_t k = i + 1; k <= j; ++k) {
            dist_sum += catalogue.GetDistance(stops[k - 1], stops[k]);
            dist_sum_inverse += catalogue.GetDistance(stops[k], stops[k - 1]);
          }
          graph.AddEdge({ bus_info->number,
            j - i,
            stop_ids.at(stop_from->name) + 1,
            stop_ids.at(stop_to->name),
            static_cast<double>(dist_sum) / (bus_velocity_ * (kDistanceFactor / kSpeedFactor))});
          if (!bus_info->is_circle) {
            graph.AddEdge({ bus_info->number,
              j - i,
              stop_ids.at(stop_to->name) + 1,
              stop_ids.at(stop_from->name),
              static_cast<double>(dist_sum_inverse) / (bus_velocity_ * (kDistanceFactor / kSpeedFactor))});
          }
        }
      }
    });
}

void Router::BuildGraph(const Catalogue& catalogue) {
  const auto& all_stops = catalogue.GetSortedAllStops();
  graph::DirectedWeightedGraph<double> stops_graph(all_stops.size() * 2);
  std::map<std::string, graph::VertexId> stop_ids;
  graph::VertexId vertex_id = 0;
  AddStopEdges(catalogue, stops_graph, stop_ids, vertex_id);
  stop_ids_ = std::move(stop_ids);
  AddBusEdges(catalogue, stops_graph, stop_ids_);
  graph_ = std::move(stops_graph);
  router_ = std::make_unique<graph::Router<double>>(graph_);
}

const std::optional<graph::Router<double>::RouteInfo> Router::FindRoute(const std::string_view stop_from, const std::string_view stop_to) const {
	return router_->BuildRoute(stop_ids_.at(std::string(stop_from)),stop_ids_.at(std::string(stop_to)));
}

const graph::DirectedWeightedGraph<double>& Router::GetGraph() const {
	return graph_;
}

}