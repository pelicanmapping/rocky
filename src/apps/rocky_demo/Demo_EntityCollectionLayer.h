/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_EntityCollectionLayer = [](Application& app)
{
#ifdef ROCKY_HAS_IMGUI
        
    static EntityCollectionLayer::Ptr layer;

    if (layer == nullptr)
    {
        layer = EntityCollectionLayer::create(app.registry);
        layer->name = "EntityCollectionLayer Demo";
        app.mapNode->map->add(layer);
        if (!layer->open(app.io()))
        {
            Log()->warn("Failed to open EntityCollectionLayer");
            return;
        }

        app.registry.write([&](entt::registry& r)
            {
                for (int i = 0; i < 10; ++i)
                {
                    // Create a new entity for the widget:
                    auto entity = r.create();

                    auto& label = r.emplace<Label>(entity);
                    label.text = "ECL Widget #" + std::to_string(i + 1);

                    auto& xform = r.emplace<Transform>(entity);
                    xform.position = GeoPoint(SRS::WGS84, -25.0 + i * 5.0, 25.0 - i * 5.0, 500'000.0);

                    layer->entities.emplace_back(entity);
                }
            });

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped("EntityCollectionLayer is a map layer managing a vector of ECS entities.", "%s",
        " Open the 'Map' panel to toggle the layer on and off.");
    

#endif
};
