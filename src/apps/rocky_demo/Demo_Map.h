/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Map.h>
#include <rocky/VisibleLayer.h>
#include <rocky/TileLayer.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Map = [](Application& app)
{
    static std::vector<bool> layerExpanded;

    // Display the map's profile
    if (app.commandLineStatus.ok())
    {
        if (ImGuiLTable::Begin("map"))
        {
            if (!app.mapNode->map->name().empty())
            {
                ImGuiLTable::Text("Name:", app.mapNode->map->name().c_str());
            }

            auto& profile = app.mapNode->profile;
            if (profile.valid())
            {
                if (!profile.wellKnownName().empty())
                    ImGuiLTable::Text("Profile:", profile.wellKnownName().c_str());
                else
                    ImGuiLTable::TextWrapped("Profile:", profile.to_json().c_str());
            }

            ImGuiLTable::End();
        }
    }
    else
    {
        ImGui::TextColored(ImGuiErrorColor, app.commandLineStatus.message.c_str());
    }

    // Enumerate all the map's layers and display information about them
    ImGui::SeparatorText("Layers");
    auto layers = app.mapNode->map->layers().all();
    layerExpanded.resize(layers.size(), false);
    int i = 0;
    bool resetTerrain = false;

    for (auto& layer : layers)
    {
        auto visibleLayer = VisibleLayer::cast(layer);
        if (visibleLayer)
        {
            ImGui::PushID(layer->uid());

            bool stylePushed = false;
            if (layer->status().failed() && layer->status().message != "Layer closed")
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 72, 72))), stylePushed = true;
            else if (!layer->isOpen())
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(127, 127, 127))), stylePushed = true;

            ImGui::PushID("selectable");
            bool layerClicked = false;
            bool open = layer->isOpen();
            if (ImGui::Checkbox("##selectable", &open))
            {
                bool resetTerrain = false;
                if (open)
                {
                    resetTerrain = layer->open(app.io()).ok();
                }
                else
                {
                    layer->close();
                    resetTerrain = true;
                }
            }

            ImGui::SameLine();

            std::string name =
                layer->name().empty() ? std::string(" Unnamed ") + layer->getLayerTypeName() + " layer" :
                layer->name();

            ImGui::Selectable(name.c_str(), &layerClicked);

            if (layerClicked)
            {
                layerExpanded[i] = !layerExpanded[i];
            }
            ImGui::PopID();

            if (layerExpanded[i])
            {
                ImGui::Indent();
                if (ImGuiLTable::Begin("layerdeets"))
                {
                    if (layer->status().failed())
                    {
                        ImGuiLTable::Text("ERROR:", layer->status().message.c_str());
                    }
                    ImGuiLTable::Text("Type:", layer->getLayerTypeName().c_str());
                    auto tileLayer = TileLayer::cast(layer);
                    if (tileLayer)
                    {
                        std::string srs_name = tileLayer->profile().srs().name();
                        if (srs_name.empty() || srs_name == "unknown")
                            srs_name = tileLayer->profile().srs().definition();
                        ImGuiLTable::Text("SRS:", srs_name.c_str());
                    }
                    const GeoExtent& extent = layer->extent();
                    if (extent.valid())
                    {
                        ImGuiLTable::TextWrapped("Extent:", "W:%.1f E:%.1f S:%.1f N:%.1f",
                            extent.west(), extent.east(), extent.south(), extent.north());
                    }
                    if (layer->attribution().has_value())
                    {
                        ImGuiLTable::TextWrapped("Attribution:", layer->attribution()->text.c_str());
                    }
                    ImGuiLTable::End();
                }
                ImGui::Unindent();
            }

            if (stylePushed)
                ImGui::PopStyleColor();

            ImGui::PopID();
            ImGui::Separator();
        }

        ++i;
    }

    if (ImGui::Button("Refresh"))
    {
        resetTerrain = true;
    }

    if (resetTerrain)
    {
        app.mapNode->terrainNode->reset(app.context);
    }
};
