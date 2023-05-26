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
                    std::string name = std::string("Unnamed ") + layer->getConfigKey() + " layer";
                    ImGui::Selectable(name.c_str(), &layerClicked);
                }
                else
                {
                    ImGui::Selectable(layer->name().c_str(), &layerClicked);
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
                ImGui::Text("Type: %s", layer->getConfigKey());
                auto tileLayer = TileLayer::cast(layer);
                if (tileLayer)
                {
                    ImGui::Text("SRS: %s", tileLayer->profile().srs().name());
                }
                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Separator();
        }

        ++i;
    }
};
