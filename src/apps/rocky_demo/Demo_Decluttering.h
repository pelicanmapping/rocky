/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>
#include <rocky/vsg/Transform.h>
#include <rocky/vsg/Motion.h>
#include <rocky/vsg/Declutter.h>
#include <rocky/vsg/DisplayManager.h>
#include <set>
#include <random>
#include <rocky/rtree.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

namespace
{
    class DeclutterSystem
    {
    public:
        DeclutterSystem(entt::registry& in_registry)
            : registry(in_registry) { }

        entt::registry& registry;
        float update_hertz = 1.0f; // updates per second
        bool enabled = true;
        double buffer_radius = 25.0;
        bool sort_by_priority = true;

        unsigned total = 0, visible = 1;
        std::size_t last_max_size = 32;
        std::function<std::vector<std::uint32_t>()> getActiveViewIDs;

        static std::shared_ptr<DeclutterSystem> create(entt::registry& registry) {
            return std::make_shared<DeclutterSystem>(registry);
        }

        void update(Runtime& runtime)
        {
            total = 0, visible = 0;

            auto viewIDs = getActiveViewIDs(); // copy

            for(auto viewID = 0; viewID < viewIDs.size(); ++viewID)
            {
                if (viewIDs[viewID] == 0) // skip unused
                    continue;

                // First collect all declutter-able entities and sort them by their distance to the camera.
                std::vector<std::tuple<entt::entity, double, double, double, int, int>> sorted; // entity, x, y, sort_key, width, height
                sorted.reserve(last_max_size);

                double aspect_ratio = 1.0; // same for all objects
                auto view = registry.view<Declutter, Transform>();
                for (auto&& [entity, declutter, transform] : view.each())
                {
                    if (transform.node && transform.node->viewLocal.size() > viewID)
                    {
                        int width = 
                            (declutter.width_px >= 0 ? declutter.width_px : 0) +
                            (declutter.buffer_x >= 0 ? declutter.buffer_x : (int)buffer_radius);
                        int height = 
                            (declutter.height_px >= 0 ? declutter.height_px : 0) +
                            (declutter.buffer_y >= 0 ? declutter.buffer_y : (int)buffer_radius);                        

                        // Cheat by directly accessing view 0. In reality we will might either declutter per-view
                        // or have a "driving view" that controls visibility for all views.
                        auto& viewLocal = transform.node->viewLocal[viewID];
                        if (!viewLocal.culled)
                        {
                            auto& mvp = viewLocal.mvp;
                            auto clip = mvp[3] / mvp[3][3];
                            vsg::dvec2 window((clip.x + 1.0) * 0.5 * (double)viewLocal.viewport[2], (clip.y + 1.0) * 0.5 * (double)viewLocal.viewport[3]);
                            double sort_key = sort_by_priority ? (double)declutter.priority : clip.z;
                            sorted.emplace_back(entity, window.x, window.y, sort_key, width, height);
                        }
                    }
                }

                // sort them by whatever sort key we used, either priority or camera distance
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return std::get<3>(a) > std::get<3>(b); });
                last_max_size = sorted.size();

                // Next, take the sorted vector and declutter by populating an R-Tree with rectangles representing
                // each entity's buffered location in screen(clip) space. For objects that don't conflict with
                // higher-priority objects, set visibility to true.
                RTree<entt::entity, double, 2> rtree;

                for (auto iter : sorted)
                {
                    ++total;

                    auto [entity, x, y, sort_key, width, height] = iter;

                    auto& visibility = registry.get<Visibility>(entity);
                    if (visibility.parent == nullptr)
                    {
                        double half_width = (double)(width >> 1);
                        double half_height = (double)(height >> 1);

                        double LL[2]{ x - half_width, y - half_height * aspect_ratio };
                        double UR[2]{ x + half_width, y + half_height * aspect_ratio };

                        if (rtree.Search(LL, UR, [](auto e) { return false; }) == 0)
                        {
                            rtree.Insert(LL, UR, entity);
                            visibility[viewID] = true;
                            ++visible;
                        }
                        else
                        {
                            visibility[viewID] = false;
                        }
                    }
                }
            }
        }

        void resetVisibility()
        {
            auto view = registry.view<Declutter, Visibility>();
            for (auto&& [entity, declutter, visibility] : view.each())
            {
                visibility.setAll(true);
            }
        }
    };
}

auto Demo_Decluttering = [](Application& app)
{
    static Status status;
    static std::shared_ptr<DeclutterSystem> declutter;

    if (!declutter)
    {
        declutter = DeclutterSystem::create(app.entities);

        // tell the declutterer how to access view IDs.
        declutter->getActiveViewIDs = [&app]() { return app.displayManager->activeViewIDs; };

        app.backgroundServices.start("rocky::declutter",
            [&app](jobs::cancelable& token)
            {
                Log()->info("Declutter thread starting.");
                while (!token.canceled())
                {
                    run_at_frequency f(declutter->update_hertz);
                    if (declutter->enabled)
                    {
                        declutter->update(app.runtime());
                        app.runtime().requestFrame();
                    }
                }
                Log()->info("Declutter thread terminating.");
            });
    }

    if (ImGuiLTable::Begin("declutter"))
    {
        if (ImGuiLTable::Checkbox("Enabled", &declutter->enabled)) {
            if (!declutter->enabled)
                declutter->resetVisibility();
        }

        ImGuiLTable::Checkbox("Sort by priority", &declutter->sort_by_priority);
        ImGuiLTable::SliderDouble("Radius", &declutter->buffer_radius, 0.0f, 50.0f, "%.0f px");
        ImGuiLTable::SliderFloat("Frequency", &declutter->update_hertz, 1.0f, 30.0f, "%.0f hz");

        if (declutter->enabled)
        {
            ImGuiLTable::Text("Candidates", "%ld / %ld", declutter->visible, declutter->total);
        }

        ImGuiLTable::End();
    }
};
