/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Map.h>
#include <rocky/VisibleLayer.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Map = [](Application& app)
{
    static std::vector<bool> layerExpanded;

    auto& profile = app.map()->profile();
    if (profile.valid())
    {
        if (!profile.wellKnownName().empty())
            ImGui::Text("Profile: %s", profile.wellKnownName());
        else {
            ImGui::Text("Profile:");
            ImGui::Text("%s", profile.to_json());
        }
    }

    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Layers:");
    auto& layers = app.map()->layers().all();
    layerExpanded.resize(layers.size(), false);
    int i = 0;

    for (auto& layer : layers)
    {
        auto visibleLayer = VisibleLayer::cast(layer);
        if (visibleLayer)
        {
            ImGui::PushID(layer->uid());

            //bool visible = visibleLayer->visible();
            //if (ImGui::Checkbox("", &visible))
            //{
            //    visibleLayer->setVisible(visible);
            //}
            //ImGui::SameLine();

            {
                ImGui::PushID("selectable");
                bool layerClicked = false;
                if (layer->name().empty())
                {
                    std::string name = std::string("- Unnamed ") + layer->getConfigKey() + " layer";
                    ImGui::Selectable(name.c_str(), &layerClicked);
                }
                else
                {
                    std::string name = std::string("- ") + layer->name();
                    ImGui::Selectable(name.c_str(), &layerClicked);
                }

                if (layerClicked)
                {
                    layerExpanded[i] = !layerExpanded[i];
                }
                ImGui::PopID();
            }

            if (layerExpanded[i])
            {
                ImGui::Indent();
                if (ImGuiLTable::Begin("layerdeets"))
                {
                    ImGuiLTable::Text("Type:", layer->getConfigKey().c_str());
                    auto tileLayer = TileLayer::cast(layer);
                    if (tileLayer)
                    {
                        ImGuiLTable::Text("SRS:", tileLayer->profile().srs().name());
                    }
                    const GeoExtent& extent = layer->extent();
                    if (extent.valid())
                    {
                        ImGuiLTable::TextWrapped("Extent:", "W:%.1f E:%.1f S:%.1f N:%.1f",
                            extent.west(), extent.east(), extent.south(), extent.north());
                    }
                    ImGuiLTable::End();
                }
                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Separator();
        }

        ++i;
    }
};
