/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

namespace
{
    struct DemoWidgetMouseHandler : public vsg::Inherit<vsg::Visitor, DemoWidgetMouseHandler>
    {
        Application& app;
        DemoWidgetMouseHandler(Application& in_app) : app(in_app) {}
        std::optional<vsg::ButtonPressEvent> _press;

        Callback<void(const GeoPoint&)> onClick;

        void apply(vsg::ButtonPressEvent& e) override
        {
            if (e.button == 1) // left mouse button
            {
                _press = e;
            }
        }

        void apply(vsg::ButtonReleaseEvent& e) override
        {
            if (_press.has_value())
            {
                auto dx = std::abs(e.x - _press->x), dy = std::abs(e.y - _press->y);
                if (dx < 5 && dy < 5) // click threshold
                {
                    auto view = app.display.getView(_press->window, _press->x, _press->y);
                    if (view)
                    {
                        if (auto p = rocky::pointAtWindowCoords(view, _press->x, _press->y))
                        {
                            onClick.fire(p.value());
                            e.handled = true;
                        }
                    }
                }
            }
            _press.reset();
        }
    };
}

auto Demo_Widget = [](Application& app)
{
#ifdef ROCKY_HAS_IMGUI
        
    static entt::entity entity = entt::null;
    static bool move_widget_on_map_click = false;
    static CallbackSub sub;

    if (entity == entt::null)
    {
        auto handler = DemoWidgetMouseHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        sub = handler->onClick([&](const GeoPoint& p)
            {
                if (move_widget_on_map_click)
                {
                    app.registry.read([&](entt::registry& registry)
                        {
                            auto& transform = registry.get<Transform>(entity);
                            auto old = transform.position.transform(SRS::WGS84);
                            auto pos = p.transform(SRS::WGS84);
                            pos.z = old.z;
                            transform.position = pos;
                            transform.dirty();
                        });
                }
            });


        auto [lock, registry] = app.registry.write();

        entity = registry.create();

        auto& widget = registry.emplace<Widget>(entity);
        widget.text = "I'm a widget.";

        widget.render = [&](WidgetInstance& i)
            {
                static float some_float = 0;
                static int some_int = 0;
                static bool fixed_window_open = false;

                i.begin();

                i.windowFlags &= ~ImGuiWindowFlags_NoInputs;
                i.windowFlags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
                ImGui::SetNextWindowBgAlpha(1.0f);

                i.render([&]()
                    {
                        ImGui::Text(i.widget.text.c_str());
                        ImGui::Separator();
                        ImGui::Text("Text string");
                        ImGui::SliderFloat("Slider", &some_float, 0.0f, 1.0f);
                        ImGui::Checkbox("Show me a fixed-position window", &fixed_window_open);
                    });

                ImGui::PopStyleVar();

                if (fixed_window_open)
                {
                    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x - 400, ImGui::GetMainViewport()->Size.y - 200));

                    if (ImGui::Begin("Fixed-position window"))
                    {
                        Transform& transform = i.registry.get<Transform>(i.entity);
                        auto p = transform.position.transform(SRS::WGS84);

                        ImGui::Text("Widgets can be placed at absolute coordinates too.");
                        ImGui::Separator();
                        if (ImGuiLTable::Begin("pos_table")) {
                            ImGuiLTable::Text("Latitude:", "%.3f", p.y);
                            ImGuiLTable::Text("Longitude:", "%.3f", p.x);
                            ImGuiLTable::Text("Altitude:", "%.1f", p.z);
                            ImGuiLTable::End();
                        }
                        ImGui::Checkbox("Move widget on map click", &move_widget_on_map_click);
                        ImGui::Separator();
                        if (ImGui::Button("Whatever")) {
                            fixed_window_open = false;
                            move_widget_on_map_click = false;
                        }
                    }
                    ImGui::End();
                }

                i.end();
            };

        // Attach a transform to place and move the label:
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, -25.0, 25.0, 2'500'000.0);
        transform.topocentric = true;

        // Drop line from the widget to the ground, for fun.
        auto& dropline = registry.emplace<Line>(entity);
        dropline.points = { { 0,0,0 }, { 0, 0, -2'500'000.0 } };
        dropline.style.color = Color(0.1f, 0.1f, 0.1f, 1.0f);
        dropline.style.width = 2;
    }

    if (ImGuiLTable::Begin("widget_demo"))
    {
        auto [lock, registry] = app.registry.read();

        bool v = visible(registry, entity);
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            setVisible(registry, entity, v);
        }

        auto& widget = registry.get<Widget>(entity);

        if (widget.text.length() <= 255)
        {
            char buf[256];
            std::copy(widget.text.begin(), widget.text.end(), buf);
            buf[widget.text.length()] = '\0';
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                widget.text = std::string(buf);
            }
        }

        auto& transform = registry.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2'500'000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }

#endif
};
