
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Common.h>

#ifdef ROCKY_HAS_IMGUI

#include "WidgetSystem.h"
#include "TransformDetail.h"
#include <rocky/Rendering.h>
#include <rocky/ecs/Widget.h>
#include <rocky/ecs/Visibility.h>
#include <imgui.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    // Internal component for rendering a widget instance
    struct WidgetRenderable
    {
        const std::string uid = std::to_string((std::uintptr_t)this);
        ViewLocal<ImVec2> screen;
        ImVec2 windowSize = { -1, -1 };
        WidgetRenderable() {
            screen.fill({ -1.0, -1.0 });
        }
    };

    void on_construct_Widget(entt::registry& r, entt::entity e)
    {
        (void)r.get_or_emplace<ActiveState>(e);
        (void)r.get_or_emplace<Visibility>(e);

        r.emplace<WidgetRenderable>(e);
    }

    void on_destroy_Widget(entt::registry& r, entt::entity e)
    {
        r.remove<WidgetRenderable>(e);
    }
}


WidgetSystemNode::WidgetSystemNode(Registry& in_registry) :
    vsg::Inherit<vsg::Node, WidgetSystemNode>(),
    System(in_registry)
{
    // configure EnTT to automatically add the necessary components when a Widget is constructed
    auto [lock, registry] = _registry.write();

    registry.on_construct<Widget>().connect<&on_construct_Widget>();
    registry.on_destroy<Widget>().connect<&on_destroy_Widget>();
}

void
WidgetSystemNode::initialize(VSGContext context)
{
    // register me as a gui rendering callback.
    auto recorder = [this](RenderingState& rs, void* imguiContext)
        {
            auto [lock, reg] = _registry.read();

            const int defaultWindowFlags =
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoSavedSettings
#ifdef IMGUI_HAS_DOCK
                | ImGuiWindowFlags_NoDocking
#endif
                ;

            auto view = reg.view<Widget, WidgetRenderable, TransformDetail, Visibility, ActiveState>();
            for (auto&& [entity, widget, renderable, xdetail, visibility, active] : view.each())
            {
                if (visible(visibility, rs) && xdetail.passingCull(rs))
                {
                    if (widget.render)
                    {
                        WidgetInstance i{
                               widget,
                               renderable.uid,
                               reg,
                               entity,
                               defaultWindowFlags,
                               renderable.screen[rs.viewID],
                               (ImGuiContext*)imguiContext,
                               rs.viewID
                        };

                        // Note: widget render needs to call ImGui::SetCurrentContext(i.context)
                        // because of the DLL boundary
                        widget.render(i);
                    }
                }
            }
        };

    context->guiRecorders.emplace_back(recorder);
}

void
WidgetSystemNode::update(VSGContext context)
{
    auto [lock, registry] = _registry.read();

    // calculate the screen position of the widget in each view
    registry.view<WidgetRenderable, TransformDetail>().each([&](auto& renderable, auto& xdetail)
        {
            for(auto& viewID : context->activeViewIDs)
            {
                auto& view = xdetail.views[viewID];
                auto clip = view.mvp[3] / view.mvp[3][3];
                renderable.screen[viewID].x = (clip.x + 1.0) * 0.5 * (double)view.viewport[2] + (double)view.viewport[0];
                renderable.screen[viewID].y = (clip.y + 1.0) * 0.5 * (double)view.viewport[3] + (double)view.viewport[1];
            }
        });
}

#endif // ROCKY_HAS_IMGUI
