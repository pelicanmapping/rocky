/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs.h>
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
        DeclutterSystem(ecs::Registry& in_registry)
            : _registry(in_registry) { }

        float update_hertz = 1.0f; // updates per second
        bool enabled = true;
        double buffer_px = 25.0;
        int sorting_method = 0; // 0=priority, 1=distance
        unsigned visible = 1;
        unsigned total = 0;
        std::function<std::vector<std::uint32_t>()> getActiveViewIDs;

        ecs::Registry& _registry;
        std::size_t _last_max_size = 32;

        static auto create(ecs::Registry& registry) {
            return std::make_shared<DeclutterSystem>(registry);
        }

        void update(VSGContext& runtime)
        {
            total = 0, visible = 0;

            auto viewIDs = getActiveViewIDs(); // copy

            for(auto viewID = 0; viewID < viewIDs.size(); ++viewID)
            {
                if (viewIDs[viewID] == 0) // skip unused
                    continue;

                // First collect all declutter-able entities and sort them by their distance to the camera.
                // tuple = [entity, x, y, sort_key, width, height]
                std::vector<std::tuple<entt::entity, double, double, double, int, int>> sorted;
                sorted.reserve(_last_max_size);

                auto [lock, registry] = _registry.read();

                auto view = registry.view<ActiveState, Declutter, Transform>();
                for (auto&& [entity, active, declutter, transform] : view.each())
                {
                    if (transform.node && transform.node->viewLocal.size() > viewID)
                    {
                        int width =
                            (declutter.width_px >= 0 ? declutter.width_px : 0) +
                            (declutter.buffer_x >= 0 ? declutter.buffer_x : (int)buffer_px);
                        int height =
                            (declutter.height_px >= 0 ? declutter.height_px : 0) +
                            (declutter.buffer_y >= 0 ? declutter.buffer_y : (int)buffer_px);

                        // Cheat by directly accessing view 0. In reality we will might either declutter per-view
                        // or have a "driving view" that controls visibility for all views.
                        auto& viewLocal = transform.node->viewLocal[viewID];
                        if (!viewLocal.culled)
                        {
                            vsg::mat4 mvp;
                            ROCKY_FAST_MAT4_MULT(mvp, viewLocal.proj, viewLocal.modelview);
                            //auto& mvp = viewLocal.mvp;
                            auto clip = mvp[3] / mvp[3][3];
                            vsg::dvec2 window((clip.x + 1.0) * 0.5 * (double)viewLocal.viewport[2], (clip.y + 1.0) * 0.5 * (double)viewLocal.viewport[3]);
                            double sort_key = sorting_method == 0 ? (double)declutter.priority : clip.z;
                            sorted.emplace_back(entity, window.x, window.y, sort_key, width, height);
                        }
                    }
                }

                // sort them by whatever sort key we used, either priority or camera distance
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return std::get<3>(a) > std::get<3>(b); });
                _last_max_size = sorted.size();

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

                        double LL[2]{ x - half_width, y - half_height };
                        double UR[2]{ x + half_width, y + half_height };

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
            auto [lock, registry] = _registry.read();

            auto view = registry.view<Declutter, Visibility>();
            for (auto&& [entity, declutter, visibility] : view.each())
            {
                visibility.fill(true);
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
        declutter = DeclutterSystem::create(app.registry);

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
                        declutter->update(app.context);
                        app.context->requestFrame();
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

        static const char* sorting[] = { "Priority", "Distance" };
        ImGuiLTable::Combo("Sort by", (int*)&declutter->sorting_method, sorting, 2);

        ImGuiLTable::SliderDouble("Buffer", &declutter->buffer_px, 0.0f, 50.0f, "%.0f px");
        ImGuiLTable::SliderFloat("Frequency", &declutter->update_hertz, 1.0f, 30.0f, "%.0f hz");

        if (declutter->enabled)
        {
            ImGuiLTable::Text("Candidates", "%ld / %ld", declutter->visible, declutter->total);
        }

        ImGuiLTable::End();
    }
};
