/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/LineString.h>
#include <rocky_vsg/FeatureView.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_LineFeatures= [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<FeatureView> feature_view;
    static bool visible = true;

    if (!object)
    {
        ImGui::Text("Wait...");

        auto fs = rocky::OGRFeatureSource::create();
        fs->uri = "H:/devel/osgearth/master/repo/data/world.shp";
        auto fs_status = fs->open();
        ROCKY_HARD_ASSERT(fs_status.ok());

        feature_view = FeatureView::create();
        auto iter = fs->iterate(app.instance.ioOptions());
        while (iter->hasMore())
        {
            auto feature = iter->next();
            if (feature.valid())
            {
                feature_view->features.emplace_back(std::move(feature));
            }
        }

        feature_view->styles.line = LineStyle{ { 1,1,0.3,1 }, 2.0f, 0xffff, 1, 100000.0f };

        // Finally, create an object with our attachment.
        app.add(object = MapObject::create(feature_view));
        return;
    }

    if (ImGuiLTable::Begin("Line features"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        if (!feature_view->attachments.empty())
        {
            auto line = MultiLineString::cast(feature_view->attachments.front());
            if (line)
            {
                LineStyle style = line->style();
                if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.0f"))
                {
                    for (auto& a : feature_view->attachments) {
                        auto temp = MultiLineString::cast(a);
                        if (temp) temp->setStyle(style);
                    }
                }
            }

#if 0
            if (ImGuiLTable::SliderFloat("Depth offset", &style.depth_offset, 0.0, 0.001, "%.8f", ImGuiSliderFlags_Logarithmic))
            {
                line->setStyle(style);
            }

            if (ImGuiLTable::Button("Sample DO"))
            {
                auto view = app.displayConfiguration.windows.begin()->second.front();
                auto lookat = view->camera->viewMatrix.cast<vsg::LookAt>();
                double mag = vsg::length(lookat->eye);
                auto down_unit = -vsg::normalize(lookat->eye);
                auto look_unit = vsg::normalize(lookat->center - lookat->eye);
                double dot = vsg::dot(down_unit, look_unit);
                std::cout
                    << std::setprecision(8) << mag << ", "
                    << dot << ", "
                    << std::fixed << style.depth_offset << ", "
                    << std::endl;
            }

            static bool auto_do = false;
            if (ImGuiLTable::Checkbox("Auto DO", &auto_do))
            {
                std::cout << "Auto-DO is not yet implemented" << std::endl;
            }
#endif
        }

        ImGuiLTable::End();
    }
};
