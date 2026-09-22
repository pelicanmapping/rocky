/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Geocoder.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Writes the source polygon, holes, and attributes in WGS84 GeoJSON, replacing path.
    //! Works on a copy of the feature and reports conversion, serialization, and file errors.
    Result<> export_geocoder_polygon(const Feature& source, const std::filesystem::path& path)
    {
        using json = nlohmann::json;
        try
        {
            if (source.geometry.type != Geometry::Type::Polygon &&
                source.geometry.type != Geometry::Type::MultiPolygon)
                return Failure("Select a polygon result to export");

            Feature feature = source;
            if (!feature.transformInPlace(SRS::WGS84))
                return Failure("Cannot transform the polygon to WGS84");

            // GeoJSON requires an explicit closing coordinate and at least three ring vertices.
            const auto ring_json = [](const std::vector<glm::dvec3>& points)
            {
                auto size = points.size();
                if (size > 1u && points.front() == points.back())
                    --size;
                if (size < 3u)
                    throw std::runtime_error("Polygon ring has fewer than three vertices");

                auto ring = json::array();
                for (std::size_t i = 0u; i < size; ++i)
                {
                    const auto& p = points[i];
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                        throw std::runtime_error("Polygon contains a non-finite coordinate");
                    ring.push_back({ p.x, p.y, p.z });
                }
                ring.push_back({ points.front().x, points.front().y, points.front().z });
                return ring;
            };

            auto polygons = json::array();
            Geometry::const_iterator(feature.geometry, false).eachPart([&](const Geometry& polygon)
                {
                    auto rings = json::array();
                    rings.push_back(ring_json(polygon.points));
                    for (const auto& hole : polygon.parts)
                        rings.push_back(ring_json(hole.points));
                    polygons.push_back(std::move(rings));
                });
            if (polygons.empty())
                return Failure("Polygon has no coordinates");

            auto properties = json::object();
            for (const auto& field : feature.fields)
            {
                // Preserve attribute types, including unset fields as JSON null.
                std::visit([&](const auto& value)
                    {
                        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::monostate>)
                            properties[field.first] = nullptr;
                        else
                            properties[field.first] = value;
                    }, static_cast<const Feature::FieldValueUnion&>(field.second));
            }

            const bool multi = feature.geometry.type == Geometry::Type::MultiPolygon;
            json document = {
                { "type", "Feature" },
                { "properties", std::move(properties) },
                { "geometry", {
                    { "type", multi ? "MultiPolygon" : "Polygon" },
                    { "coordinates", multi ? polygons : polygons.front() }
                } }
            };
            if (feature.id >= 0)
                document["id"] = feature.id;

            // Serialize before opening the destination so conversion errors cannot truncate it.
            const auto serialized = document.dump(2);
            std::ofstream output(path);
            if (!output)
                return Failure("Cannot open " + path.string());
            output << serialized << '\n';
            output.close();
            if (!output)
                return Failure("Failed to write " + path.string());
            return ResultVoidOK;
        }
        catch (const std::exception& error)
        {
            return Failure(error.what());
        }
    }
}

auto Demo_Geocoder = [](Application& app)
{
    struct Placemark
    {
        entt::entity label = entt::null;
        entt::entity outline = entt::null;

        void create(entt::registry& reg)
        {
            // configure a line that will display the outline of the selected place:
            outline = reg.create();
            reg.emplace<Polygon>(outline);
            reg.emplace<Line>(outline);
            reg.emplace<Overlay>(outline);

            // configure a label for the selected place:
            label = reg.create();
            reg.emplace<Label>(label, "");
            reg.emplace<Transform>(label);
        }

        void show(entt::registry& r, bool toggle)
        {
            r.get<Visibility>(outline).visible = toggle;
            r.get<Visibility>(label).visible = toggle;
        }
    };

    static Placemark placemark;

    static Future<Result<std::vector<Feature>>> geocoding_task;
    static char input_buf[256];
    static int selected_result = -1;
    static std::string export_message;

    if (placemark.label == entt::null)
    {
        app.registry.write([&](entt::registry& reg)
            {
                // configure some graphics to represent the selected place,
                // and make them invisible to start.
                placemark.create(reg);
                placemark.show(reg, false);
            });

        app.vsgcontext->requestFrame();
    }

    else
    {
        if (ImGuiLTable::Begin("geocoding"))
        {
            if (ImGuiLTable::InputText("Location:", input_buf, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                selected_result = -1;
                export_message.clear();
                // hide the graphics:
                app.registry.read([&](entt::registry& reg) {
                        placemark.show(reg, false);
                    });

                std::string input(input_buf);

                geocoding_task = app.io().services().jobs.dispatch([&app, input](jobs::cancelable& c) -> Result<std::vector<Feature>>
                    {
                        if (c.canceled())
                            return Failure_OperationCanceled;

                        Geocoder geocoder;
                        auto r = geocoder.geocode(input, app.io());
                        app.vsgcontext->requestFrame();
                        return r;
                    });

                app.vsgcontext->requestFrame();
            }
            ImGuiLTable::End();
        }

        if (geocoding_task.working())
        {
            ImGui::TextUnformatted("Searching...");
        }

        else if (geocoding_task.available())
        {
            auto result = geocoding_task.value();
            if (result.ok())
            {
                int count = 0;
                ImGui::TextUnformatted("Click on a result to center:");

                for (auto& feature : result.value())
                {
                    ImGui::PushID(count++);
                    bool selected = false;
                    ImGui::Separator();
                    auto display_name = feature.field("display_name").stringValue();
                    ImGui::Selectable(display_name.c_str(), &selected);
                    if (selected)
                    {
                        selected_result = count - 1;
                        export_message.clear();
                        app.onNextUpdate([&app, myfeature(feature), display_name](...) mutable
                            {
                                auto extent = myfeature.extent;
                                if (extent.area() == 0.0)
                                    extent.expand(Distance(10, Units::KILOMETERS), Distance(10, Units::KILOMETERS));

                                auto& view = app.display.window(0).view(0);
                                if (auto manip = MapManipulator::get(view.vsgView))
                                {
                                    Viewpoint vp = manip->viewpoint();
                                    vp.point = extent.centroid();
                                    vp.range = Distance(std::max(extent.width(Units::METERS) * 7.0, 2500.0), Units::METERS);
                                    manip->setViewpoint(vp, std::chrono::seconds(2));
                                }

                                if (myfeature.geometry.type != Geometry::Type::Points)
                                {
                                    // Outline for location boundary:
                                    FeatureBuilder builder;
                                    
                                    PolygonStyle workingPolygonStyle;
                                    workingPolygonStyle.color = Color(StockColor::White, 0.35f);

                                    LineStyle workingLineStyle;
                                    workingLineStyle.color = Color(StockColor::White, 1.0f);
                                    workingLineStyle.width = 2.0f;

                                    PolygonGeometry workingPolygonGeom;
                                    builder.buildPolygonGeometry({ myfeature }, workingPolygonStyle, workingPolygonGeom);

                                    LineGeometry workingLineGeom;
                                    builder.buildLineGeometry({ myfeature }, workingLineStyle, workingLineGeom);

                                    app.registry.write([&](entt::registry& r)
                                        {
                                            auto& polygonGeom = r.emplace_or_replace<PolygonGeometry>(placemark.outline, std::move(workingPolygonGeom));
                                            auto& polygonStyle = r.emplace_or_replace<PolygonStyle>(placemark.outline, std::move(workingPolygonStyle));
                                            r.emplace_or_replace<Polygon>(placemark.outline, polygonGeom, polygonStyle);

                                            auto& lineGeom = r.emplace_or_replace<LineGeometry>(placemark.outline, std::move(workingLineGeom));
                                            auto& lineStyle = r.emplace_or_replace<LineStyle>(placemark.outline, std::move(workingLineStyle));
                                            r.emplace_or_replace<Line>(placemark.outline, lineGeom, lineStyle);

                                            placemark.show(r, true);

                                            // update the label and the transform:
                                            auto&& [xform, label] = r.get<Transform, Label>(placemark.label);

                                            auto text = display_name;
                                            rocky::detail::replaceInPlace(text, ", ", "\n");
                                            label.text = text;

                                            xform.position = myfeature.extent.centroid();
                                            xform.dirty(r);
                                        });
                                }
                            });
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();
                if (ImGui::Button("Clear"))
                {
                    geocoding_task.reset();
                    input_buf[0] = (char)0;
                    selected_result = -1;
                    export_message.clear();

                    app.registry.read([&](entt::registry& reg) {
                            placemark.show(reg, false);
                        });
                }
                ImGui::SameLine();
                const bool can_export = selected_result >= 0 &&
                    static_cast<std::size_t>(selected_result) < result.value().size() &&
                    (result.value()[selected_result].geometry.type == Geometry::Type::Polygon ||
                     result.value()[selected_result].geometry.type == Geometry::Type::MultiPolygon);
                ImGui::BeginDisabled(!can_export);
                if (ImGui::Button("Export GeoJSON"))
                {
                    std::error_code error;
                    const auto path = std::filesystem::absolute("geocoder.geojson", error);
                    if (error)
                        export_message = "Export failed: " + error.message();
                    else
                    {
                        auto exported = export_geocoder_polygon(result.value()[selected_result], path);
                        export_message = exported.ok() ? "Exported to " + path.string() :
                            "Export failed: " + exported.error().message;
                    }
                }
                ImGui::EndDisabled();
                if (!export_message.empty())
                    ImGui::TextWrapped("%s", export_message.c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.5, 0.5, 1), "Geocoding failed! %s", result.error().message.c_str());
            }
        }
    }
};
