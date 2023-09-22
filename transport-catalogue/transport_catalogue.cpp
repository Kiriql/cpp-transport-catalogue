#include "transport_catalogue.h"

namespace transport {
    void Catalogue::AddStop(std::string_view stop_name, const geo::Coordinates& coordinates) {
        all_stops_.push_back(Stop{ std::string(stop_name), coordinates, {} });
        stopname_to_stop_[all_stops_.back().name] = &all_stops_.back();
    }

    void Catalogue::AddRoute(std::string_view bus_number, const std::vector<const Stop*>& stops, bool is_circle) {
        all_buses_.push_back(Bus{ std::string(bus_number), stops, is_circle });
        busname_to_bus_[all_buses_.back().number] = &all_buses_.back();
        for (const auto* route_stop : stops) {
            for (auto& stop : all_stops_) {
                if (stop.name == route_stop->name) {
                    stop.buses_by_stop.insert(std::string(bus_number));
                }
            }
        }
    }

    const Bus* Catalogue::FindRoute(std::string_view bus_number) const {
        auto it = busname_to_bus_.find(bus_number);
        return it != busname_to_bus_.end() ? it->second : nullptr;
    }

    const Stop* Catalogue::FindStop(std::string_view stop_name) const {
        auto it = stopname_to_stop_.find(stop_name);
        return it != stopname_to_stop_.end() ? it->second : nullptr;
    }

    size_t Catalogue::UniqueStopsCount(std::string_view bus_number) const {
        std::set<std::string_view> unique_stops;
        const auto* bus = FindRoute(bus_number);
        if (!bus) {
            return 0;
        }
        for (const auto* stop : bus->stops) {
            unique_stops.insert(stop->name);
        }
        return unique_stops.size();
    }

    void Catalogue::SetDistance(const Stop* from, const Stop* to, const int distance) {
        stop_distances_[{from, to}] = distance;
    }

    int Catalogue::GetDistance(const Stop* from, const Stop* to) const {
        auto it = stop_distances_.find({ from, to });
        if (it != stop_distances_.end()) {
            return it->second;
        } else {
            it = stop_distances_.find({ to, from });
            return it != stop_distances_.end() ? it->second : 0;
        }
    }

    const std::map<std::string_view, const Bus*> Catalogue::GetSortedAllBuses() const {
        std::map<std::string_view, const Bus*> result;
        for (const auto& [bus_number, bus] : busname_to_bus_) {
            result.emplace(bus_number, bus);
        }
        return result;
    }

    const std::map<std::string_view, const Stop*> Catalogue::GetSortedAllStops() const {
        std::map<std::string_view, const Stop*> result;
        for (const auto& [stop_name, stop] : stopname_to_stop_) {
            result.emplace(stop_name, stop);
        }
        return result;
    }

    std::optional<transport::BusStat> Catalogue::GetBusStat(std::string_view bus_number) const {
        std::optional<transport::BusStat> bus_stat;
        const transport::Bus* bus = FindRoute(bus_number);
        if (!bus) {
            return bus_stat;
        }
        if (bus->is_circle) {
            (*bus_stat).stops_count = bus->stops.size();
        } else {
            (*bus_stat).stops_count = bus->stops.size() * 2 - 1;
        }
        int route_length = 0;
        double geographic_length = 0.0;
        for (auto it = bus->stops.begin(); it != bus->stops.end() - 1; ++it) {
            const auto* from = *it;
            const auto* to = *(it + 1);
            if (bus->is_circle) {
                route_length += GetDistance(from, to);
                geographic_length += geo::ComputeDistance(from->coordinates, to->coordinates);
            } else {
                route_length += GetDistance(from, to) + GetDistance(to, from);
                geographic_length += geo::ComputeDistance(from->coordinates, to->coordinates) * 2;
            }
        }
        (*bus_stat).unique_stops_count = UniqueStopsCount(bus_number);
        (*bus_stat).route_length = route_length;
        (*bus_stat).curvature = route_length / geographic_length;
        return bus_stat;
    }

    const std::unordered_map<std::pair<const Stop*, const Stop*>, int, Catalogue::StopDistancesHasher>& Catalogue::GetStopDistances() const {
        return stop_distances_;
    }
}