/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include "helpers.h"
using namespace ROCKY_NAMESPACE;


auto Demo_Views = [=](Application& app)
{
    static vsg::ref_ptr<vsg::View> view;
    static bool visible = true;

    auto window = app.viewer->windows().front();

    if (!view)
    {
        ImGui::Text("Wait...");

        const double nearFarRatio = 0.00001;
        double R = app.mapNode->mapSRS().ellipsoid().semiMajorAxis();

        int win_width = window->extent2D().width, win_height = window->extent2D().height;
        int x = win_width - 600, y = 50, width = 550, height = 350;

        auto perspective = vsg::Perspective::create(
            30.0,
            (double)width / (double)height,
            R * nearFarRatio,
            R * 20.0);

        auto camera = vsg::Camera::create(
            perspective,
            vsg::LookAt::create(),
            vsg::ViewportState::create(x, y, width, height));

        view = vsg::View::create(camera, app.root);

        view->viewDependentState->viewportData->properties.dataVariance = vsg::DYNAMIC_DATA;

        app.addView(window, view);
    }
    else
    {
        ImGuiLTable::Begin("add_remove_view");

        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.addView(window, view);
            else
                app.removeView(window, view);
        }

        if (visible)
        {
            auto rendergraph = app.renderGraph(view);
            if (rendergraph)
            {
                auto& cv = rendergraph->clearValues[0];
                float col[3] = { cv.color.float32[0], cv.color.float32[1], cv.color.float32[2] };
                if (ImGuiLTable::ColorEdit3("Clear", &col[0]))
                {
                    // just assume the first clear value is the color attachment :)
                    rendergraph->setClearValues({ { col[0], col[1], col[2], 1.0f } });
                }
            }

            // TODO: doesn't work!
            if (view->camera->viewportState->viewports.size() > 0)
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not yet implemented");
                auto vp = view->camera->getViewport();
                bool dirty = false;

                if (ImGuiLTable::SliderFloat("X", &vp.x, 0, window->extent2D().width, "%.0f"))
                {
                    view->camera->viewportState->viewports[0].x = vp.x, dirty = true;
                }
                if (ImGuiLTable::SliderFloat("Y", &vp.y, 0, window->extent2D().height, "%.0f"))
                {
                    view->camera->viewportState->viewports[0].y = vp.y, dirty = true;
                }
                if (ImGuiLTable::SliderFloat("Width", &vp.width, 50, window->extent2D().width / 2, "%.0f"))
                {
                    view->camera->viewportState->viewports[0].width = vp.width, dirty = true;
                }
                if (ImGuiLTable::SliderFloat("Height", &vp.height, 50, window->extent2D().height / 2, "%.0f"))
                {
                    view->camera->viewportState = vsg::ViewportState::create(vp.x, vp.y, vp.width, vp.height);
                    //view->camera->viewportState->viewports[0].height = vp.height, dirty = true;
                }

                // The ViewportState needs updating in the graphics pipelines that use it.
                // See: https://github.com/vsg-dev/VulkanSceneGraph/discussions/795
#if 0
                if (dirty)
                {
                    app.instance.runtime().runDuringUpdate([&]()
                        {
                            app.resize();
                        });
                }
#endif
            }
        }

        ImGuiLTable::End();
    }
};
