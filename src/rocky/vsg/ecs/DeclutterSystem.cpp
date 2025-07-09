/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "DeclutterSystem.h"
#include "Registry.h"
#include "Declutter.h"
#include "TransformDetail.h"
#include "Visibility.h"
#include <rocky/Utils.h>
#include <rocky/rtree.h>

using namespace ROCKY_NAMESPACE;

DeclutterSystem::DeclutterSystem(ecs::Registry r) : ecs::System(r)
{
    //nop
}

void
DeclutterSystem::update(VSGContext& runtime)
{
    _total = 0, _visible = 0;

    auto viewIDs = runtime->activeViewIDs; // copy
    auto frame = (std::int64_t)runtime->viewer->getFrameStamp()->frameCount;

    for (auto& viewID : viewIDs)
    {
        // First collect all declutter-able entities and sort them by a sorting metric,
        // either priority or distance to the camera.
        // tuple = [sorting-metric, entity rectangle]
        std::vector<std::tuple<double, entt::entity, Rect>> sorted;

        sorted.reserve(_last_max_size);

        auto [lock, registry] = _registry.update();

        auto view = registry.view<ActiveState, Declutter, TransformDetail>();

        for (auto&& [entity, active, declutter, transform_detail] : view.each())
        {
            auto& view = transform_detail.views[viewID];

            // skip anything that didn't pass cull since we can't see it
            if (!view.passesCull)
                continue;

            // caclulate the window-space coordinates of the transform.
            // TODO: should we include the transform radius? Or leave that to the user?
            auto clip = view.mvp[3] / view.mvp[3][3];
            vsg::dvec2 window((clip.x + 1.0) * 0.5 * (double)view.viewport[2], (clip.y + 1.0) * 0.5 * (double)view.viewport[3]);

            // expand the filling rectangle by the buffer:
            Rect rect = declutter.rect;
            rect.xmin += window.x - bufferPixels;
            rect.ymin += window.y - bufferPixels;
            rect.xmax += window.x + bufferPixels;
            rect.ymax += window.y + bufferPixels;

            double sorting_metric = sorting == Sorting::Priority? (double)declutter.priority : clip.z;

            sorted.emplace_back(sorting_metric, entity, rect);
        }

        // sort them by the metric we selected:
        std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) { return std::get<0>(lhs) > std::get<0>(rhs); });
        _last_max_size = sorted.size();

        // Next, take the sorted vector and declutter by populating an R-Tree with rectangles representing
        // each entity's buffered location in screen space. For objects that don't conflict with
        // higher-priority objects, set visibility to true.
        RTree<entt::entity, double, 2> rtree;

        for (auto& iter : sorted)
        {
            ++_total;

            auto& [sorting_key, entity, rect] = iter;

            auto& visibility = registry.get<Visibility>(entity);
            if (visibility.parent == nullptr)
            {
                double LL[2]{ rect.xmin, rect.ymin };
                double UR[2]{ rect.xmax, rect.ymax };

                if (rtree.Search(LL, UR, [](auto e) { return false; }) == 0)
                {
                    // no conflict - mark visible
                    rtree.Insert(LL, UR, entity);
                    visibility.visible[viewID] = true;
                    ++_visible;
                }
                else
                {
                    // conflict! mark invisibile
                    visibility.visible[viewID] = false;
                }
            }
        }
    }
}

std::pair<unsigned, unsigned>
DeclutterSystem::visibleAndTotal() const
{
    return { _visible, _total };
}

