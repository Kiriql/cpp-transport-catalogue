#include "json_reader.h"
#include "json_builder.h"

const json::Node& JsonReader::GetBaseRequests() const {
    const auto& root = input_.GetRoot().AsDict();
    auto it = root.find("base_requests");
    if (it == root.end()) return dummy_;
    return it->second;
}

const json::Node& JsonReader::GetStatRequests() const {
    const auto& root = input_.GetRoot().AsDict();
    auto it = root.find("stat_requests");
    if (it == root.end()) return dummy_;
    return it->second;
}

const json::Node& JsonReader::GetRenderSettings() const {
    const auto& root = input_.GetRoot().AsDict();
    auto it = root.find("render_settings");
    if (it == root.end()) return dummy_;
    return it->second;
}

void JsonReader::ProcessRequests(const json::Node& stat_requests, RequestHandler& rh) const {
    json::Array result;
    for (auto& request : stat_requests.AsArray()) {
        const auto& request_map = request.AsDict();
        const auto& type = request_map.at("type").AsString();
        if (type == "Stop") result.push_back(PrintStop(request_map, rh).AsDict());
        if (type == "Bus") result.push_back(PrintRoute(request_map, rh).AsDict());
        if (type == "Map") result.push_back(PrintMap(request_map, rh).AsDict());
    }

    json::Print(json::Document{ result }, std::cout);
}

void JsonReader::FillCatalogue(transport::Catalogue& catalogue) {
    const json::Array& arr = GetBaseRequests().AsArray();
    for (const auto& request_stops : arr) {
        const auto& request_stops_map = request_stops.AsDict();
        const auto& type = request_stops_map.at("type").AsString();
        if (type == "Stop") {
            auto [stop_name, coordinates, stop_distances] = FillStop(request_stops_map);
            catalogue.AddStop(stop_name, coordinates);
        }
    }
    FillStopDistances(catalogue);
    
    for (const auto& request_bus : arr) {
        const auto& request_bus_map = request_bus.AsDict();
        const auto& type = request_bus_map.at("type").AsString();
        if (type == "Bus") {
            auto [bus_number, stops, circular_route] = FillRoute(request_bus_map, catalogue);
            catalogue.AddRoute(bus_number, stops, circular_route);
        }
    }
}

std::tuple<std::string_view, geo::Coordinates, std::map<std::string_view, int>> JsonReader::FillStop(const json::Dict& request_map) const {
    std::string_view stop_name = request_map.at("name").AsString();
    geo::Coordinates coordinates = { request_map.at("latitude").AsDouble(), request_map.at("longitude").AsDouble() };
    std::map<std::string_view, int> stop_distances;
    auto& distances = request_map.at("road_distances").AsDict();
    for (auto& [stop_name, dist] : distances) {
        stop_distances.emplace(stop_name, dist.AsInt());
    }
    return std::make_tuple(stop_name, coordinates, stop_distances);
}

void JsonReader::FillStopDistances(transport::Catalogue& catalogue) const {
    const json::Array& arr = GetBaseRequests().AsArray();
    for (const auto& request_stops: arr) {
        const auto& request_stops_map = request_stops.AsDict();
        const auto& type = request_stops_map.at("type").AsString();
        if (type == "Stop") {
            auto [stop_name, coordinates, stop_distances] = FillStop(request_stops_map);
            for (auto& [to_name, dist] : stop_distances) {
                auto from = catalogue.FindStop(stop_name);
                auto to = catalogue.FindStop(to_name);
                catalogue.SetDistance(from, to, dist);
            }
        }
    }
}

std::tuple<std::string_view, std::vector<const transport::Stop*>, bool> JsonReader::FillRoute(const json::Dict& request_map, transport::Catalogue& catalogue) const {
    std::string_view bus_number = request_map.at("name").AsString();
    std::vector<const transport::Stop*> stops;
    for (auto& stop : request_map.at("stops").AsArray()) {
        stops.push_back(catalogue.FindStop(stop.AsString()));
    }
    bool circular_route = request_map.at("is_roundtrip").AsBool();

    return {bus_number, stops, circular_route};
}

svg::Color JsonReader::ReadColor(const json::Node& color_node){
    if (color_node.IsString()) {
        return color_node.AsString();
    } else if (color_node.IsArray()) {
        const json::Array& color_array = color_node.AsArray();
        if (color_array.size() == 3) {
            return svg::Rgb(color_array[0].AsInt(), color_array[1].AsInt(), color_array[2].AsInt());
        } else if (color_array.size() == 4) {
            return svg::Rgba(color_array[0].AsInt(), color_array[1].AsInt(), color_array[2].AsInt(), color_array[3].AsDouble());
        }
    }
    throw std::logic_error("wrong color type");
}

renderer::MapRenderer JsonReader::FillRenderSettings(const json::Dict& request_map) const {
    renderer::RenderSettings render_settings;
    render_settings.width = request_map.at("width").AsDouble();
    render_settings.height = request_map.at("height").AsDouble();
    render_settings.padding = request_map.at("padding").AsDouble();
    render_settings.stop_radius = request_map.at("stop_radius").AsDouble();
    render_settings.line_width = request_map.at("line_width").AsDouble();
    render_settings.bus_label_font_size = request_map.at("bus_label_font_size").AsInt();
    const json::Array& bus_label_offset = request_map.at("bus_label_offset").AsArray();
    render_settings.bus_label_offset = { bus_label_offset[0].AsDouble(), bus_label_offset[1].AsDouble() };
    render_settings.stop_label_font_size = request_map.at("stop_label_font_size").AsInt();
    const json::Array& stop_label_offset = request_map.at("stop_label_offset").AsArray();
    render_settings.stop_label_offset = { stop_label_offset[0].AsDouble(), stop_label_offset[1].AsDouble() };
    render_settings.underlayer_color = ReadColor(request_map.at("underlayer_color"));
    render_settings.underlayer_width = request_map.at("underlayer_width").AsDouble();
    const json::Array& color_palette = request_map.at("color_palette").AsArray();
    for (const auto& color_element : color_palette) {
        render_settings.color_palette.push_back(ReadColor(color_element));
    }
    return render_settings;
}

const json::Node JsonReader::PrintRoute(const json::Dict& request_map, RequestHandler& rh) const {
    json::Node result;
    const std::string& route_number = request_map.at("name").AsString();
    const int id = request_map.at("id").AsInt();
    
    if (!rh.IsBusNumber(route_number)) {
        result = json::Builder{}
                    .StartDict()
                        .Key("request_id").Value(id)
                        .Key("error_message").Value("not found")
                    .EndDict()
                .Build();
    }
    else {
        const auto& route_info = rh.GetBusStat(route_number);
        result = json::Builder{}
                    .StartDict()
                        .Key("request_id").Value(id)
                        .Key("curvature").Value(route_info->curvature)
                        .Key("route_length").Value(route_info->route_length)
                        .Key("stop_count").Value(static_cast<int>(route_info->stops_count))
                        .Key("unique_stop_count").Value(static_cast<int>(route_info->unique_stops_count))
                    .EndDict()
                .Build();
    }
    return result;
}

const json::Node JsonReader::PrintStop(const json::Dict& request_map, RequestHandler& rh) const {
    json::Node result;
    const std::string& stop_name = request_map.at("name").AsString();
    const int id = request_map.at("id").AsInt();
    
    if (!rh.IsStopName(stop_name)) {
        result = json::Builder{}
                    .StartDict()
                        .Key("request_id").Value(id)
                        .Key("error_message").Value("not found")
                    .EndDict()
                .Build();
    }
    else {
        json::Array buses;
        for (const auto& bus : rh.GetBusesByStop(stop_name)) {
            buses.push_back(bus);
        }
        result = json::Builder{}
                    .StartDict()
                        .Key("request_id").Value(id)
                        .Key("buses").Value(buses)
                    .EndDict()
                .Build();
    }
    return result;
}

const json::Node JsonReader::PrintMap(const json::Dict& request_map, RequestHandler& rh) const {
    json::Node result;
    const int id = request_map.at("id").AsInt();
    std::ostringstream strm;
    svg::Document map = rh.RenderMap();
    map.Render(strm);
    
    result = json::Builder{}
                .StartDict()
                    .Key("request_id").Value(id)
                    .Key("map").Value(strm.str())
                .EndDict()
            .Build();
    
    return result;
}