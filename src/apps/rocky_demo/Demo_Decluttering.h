/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs.h>
#include <rocky/vsg/ecs/DeclutterSystem.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

namespace
{
#if 0
    class DeclutterSystem
    {
    public:
        DeclutterSystem(ecs::Registry& in_registry)
            : _registry(in_registry) { }

        float update_hertz = 1.0f; // updates per second
        bool enabled = true;
        double buffer_px = 0.0;
        int sorting_method = 0; // 0=priority, 1=distance
        unsigned visible = 1;
        unsigned total = 0;

        ecs::Registry _registry;
        std::size_t _last_max_size = 32;

        static auto create(ecs::Registry& registry) {
            return std::make_shared<DeclutterSystem>(registry);
        }

        void update(VSGContext& runtime)
        {
            total = 0, visible = 0;

            auto viewIDs = runtime->activeViewIDs; // copy

            for(auto& viewID : viewIDs)
            {
                // First collect all declutter-able entities and sort them by their distance to the camera.
                // tuple = [entity, x, y, sort_key, width, height]
                std::vector<std::tuple<double, entt::entity, Rect>> sorted;
                sorted.reserve(_last_max_size);

                auto [lock, registry] = _registry.read();

                auto view = registry.view<ActiveState, Declutter, TransformDetail>();

                for (auto&& [entity, active, declutter, transform_detail] : view.each())
                {
                    auto& view = transform_detail.views[viewID];

                    auto clip = view.mvp[3] / view.mvp[3][3];
                    vsg::dvec2 window((clip.x + 1.0) * 0.5 * (double)view.viewport[2], (clip.y + 1.0) * 0.5 * (double)view.viewport[3]);

                    Rect rect = declutter.rect;
                    rect.xmin += window.x - buffer_px;
                    rect.ymin += window.y - buffer_px;
                    rect.xmax += window.x + buffer_px;
                    rect.ymax += window.y + buffer_px;

                    double sorting_key = sorting_method == 0 ? (double)declutter.priority : clip.z;

                    sorted.emplace_back(sorting_key, entity, rect);
                }

                // sort them by whatever sort key we used, either priority or camera distance
                std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) { return std::get<0>(lhs) > std::get<0>(rhs); });
                _last_max_size = sorted.size();

                // Next, take the sorted vector and declutter by populating an R-Tree with rectangles representing
                // each entity's buffered location in screen space. For objects that don't conflict with
                // higher-priority objects, set visibility to true.
                RTree<entt::entity, double, 2> rtree;

                for (auto iter : sorted)
                {
                    ++total;

                    auto& [sorting_key, entity, rect] = iter;

                    auto& visibility = registry.get<Visibility>(entity);
                    if (visibility.parent == nullptr)
                    {
                        double LL[2]{ rect.xmin, rect.ymin };
                        double UR[2]{ rect.xmax, rect.ymax };

                        if (rtree.Search(LL, UR, [](auto e) { return false; }) == 0)
                        {
                            rtree.Insert(LL, UR, entity);
                            visibility.visible[viewID] = true;
                            ++visible;
                        }
                        else
                        {
                            visibility.visible[viewID] = false;
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
                visibility.visible.fill(true);
            }
        }
    };
#endif
}

namespace
{
    void resetVisibility(ecs::Registry r)
    {
        auto [lock, registry] = r.update();

        auto view = registry.view<Declutter, Visibility>();
        for (auto&& [entity, declutter, visibility] : view.each())
        {
            visibility.visible.fill(true);
        }
    }
}

auto Demo_Decluttering = [](Application& app)
{
    static Status status;
    static std::shared_ptr<DeclutterSystem> declutter;
    static float updateHertz = 1.0f;
    static bool declutteringEnabled = true;

    if (!declutter)
    {
        declutter = DeclutterSystem::create(app.registry);

        app.backgroundServices.start("rocky::declutter", [&app](Cancelable& cancelable)
            {
                Log()->info("Declutter thread starting.");

                while (!cancelable.canceled())
                {
                    run_at_frequency f(updateHertz);

                    if (declutteringEnabled)
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
        if (ImGuiLTable::Checkbox("Enabled", &declutteringEnabled)) {
            if (!declutteringEnabled)
                resetVisibility(app.registry);
        }

        static const char* sorting[] = { "Priority", "Distance" };
        ImGuiLTable::Combo("Sort by", (int*)&declutter->sorting, sorting, 2);

        ImGuiLTable::SliderFloat("Buffer", &declutter->bufferPixels, 0.0f, 50.0f, "%.0f px");
        ImGuiLTable::SliderFloat("Frequency", &updateHertz, 1.0f, 30.0f, "%.0f hz");

        if (declutteringEnabled)
        {
            auto [visible, total] = declutter->visibleAndTotal();
            ImGuiLTable::Text("Results:", "%ld visible / %ld total", visible, total);
        }

        ImGuiLTable::End();
    }
};
