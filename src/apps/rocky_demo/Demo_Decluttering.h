/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/ecs/Registry.h>
#include <rocky/ecs/Declutter.h>
#include <rocky/vsg/ecs/DeclutterSystem.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

namespace
{
    void resetVisibility(Registry r)
    {
        auto [lock, registry] = r.read();

        auto view = registry.view<Declutter, Visibility>();
        for (auto&& [entity, declutter, visibility] : view.each())
        {
            visibility.visible.fill(true);
        }
    }
}

auto Demo_Decluttering = [](Application& app)
{
    static std::shared_ptr<DeclutterSystem> declutter;
    static float updateHertz = 1.0f;
    static bool declutteringEnabled = true;

    if (!declutter)
    {
        declutter = DeclutterSystem::create(app.registry);

        app.background.start("rocky::declutter", [&app](Cancelable& cancelable)
            {
                Log()->info("Declutter thread starting.");

                while (!cancelable.canceled())
                {
                    run_at_frequency f(updateHertz);

                    if (declutteringEnabled)
                    {
                        declutter->update(app.vsgcontext);
                        app.vsgcontext->requestFrame();
                    }
                }
                Log()->info("Declutter thread terminating.");
            });

        app.vsgcontext->requestFrame();
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
