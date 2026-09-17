
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Common.h>

#ifdef ROCKY_HAS_IMGUI

#include "WidgetSystem.h"
#include "TransformDetail.h"
#include "ECSVisitors.h"
#include <rocky/Rendering.h>
#include <rocky/ecs/Widget.h>
#include <rocky/ecs/Visibility.h>
#include <rocky/vsg/ViewDependentState.h>
#include <imgui.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    // Internal component for rendering a widget instance
    struct WidgetDetail
    {
        const std::string uid = std::to_string((std::uintptr_t)this);
        ViewLocal<ImVec2> screen;
        ImVec2 windowSize = { -1, -1 };
        WidgetDetail() {
            screen.fill({ -1.0, -1.0 });
        }
    };
}



void WidgetSystemNode::on_construct_Widget(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);

    r.emplace<WidgetDetail>(e);
}

void WidgetSystemNode::on_destroy_Widget(entt::registry& r, entt::entity e)
{
    r.remove<WidgetDetail>(e);
}


WidgetSystemNode::WidgetSystemNode(Registry& in_registry) :
    vsg::Inherit<vsg::Node, WidgetSystemNode>(),
    System(in_registry)
{
    // configure EnTT to automatically add the necessary components when a Widget is constructed
    auto [lock, registry] = _registry.write();

    registry.on_construct<Widget>().connect<&WidgetSystemNode::on_construct_Widget>(*this);
    registry.on_destroy<Widget>().connect<&WidgetSystemNode::on_destroy_Widget>(*this);
}

void
WidgetSystemNode::initialize(VSGContext context)
{
    // register me as a gui rendering callback.
    auto recorder = [this](const RenderingState& rs, void* imguiContext)
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
                | ImGuiWindowFlags_NoDocking
#endif
                ;

            _focusedEntities.clear();

            // widgets with a Transform:
            auto iter = reg.view<Widget, WidgetDetail, TransformDetail, Visibility, ActiveState>();
            for (auto&& [entity, widget, renderable, xdetail, visibility, active] : iter.each())
            {
                if (widget.render != nullptr && visible(visibility, rs) && xdetail.passingCull(rs))
                {
                    // Project after the scene's record traversal has refreshed this view.
                    // During update, the per-view transform data is still from the previous frame.
                    const auto& view = xdetail.views[rs.viewID];
                    auto clip = view.proj * view.position;
                    clip /= clip.w;
                    auto& screen = renderable.screen[rs.viewID];
                    screen.x = (clip.x + 1.0) * 0.5 * (double)view.viewport[2] + (double)view.viewport[0];
                    screen.y = (clip.y + 1.0) * 0.5 * (double)view.viewport[3] + (double)view.viewport[1];

                    WidgetInstance i{
                            widget,
                            renderable.uid,
                            reg,
                            rs,
                            entity,
                            defaultWindowFlags,
                            renderable.screen[rs.viewID],
                            (ImGuiContext*)imguiContext,
                            false // focus
                    };

                    // Note: widget render needs to call ImGui::SetCurrentContext(i.context)
                    // because of the DLL boundary
                    widget.render(i);

                    // remember any widgets that want focus.
                    if (i.hasFocus)
                    {
                        _focusedEntities.emplace(entity);
                    }
                }
            }

            // widgets WITHOUT a Transform:
            auto iter2 = reg.view<Widget, WidgetDetail, Visibility, ActiveState>(entt::exclude<TransformDetail>);
            for (auto&& [entity, widget, renderable, visibility, active] : iter2.each())
            {
                if (widget.render != nullptr && visible(visibility, rs))
                {
                    WidgetInstance i{
                            widget,
                            renderable.uid,
                            reg,
                            rs,
                            entity,
                            defaultWindowFlags,
                            renderable.screen[rs.viewID],
                            (ImGuiContext*)imguiContext,
                            false // focus
                    };

                    // Note: widget render needs to call ImGui::SetCurrentContext(i.context)
                    // because of the DLL boundary
                    widget.render(i);

                    // remember any widgets that want focus.
                    if (i.hasFocus)
                    {
                        _focusedEntities.emplace(entity);
                    }
                }
            }
        };

    context->guiRecorders.emplace_back(recorder);
}

void
WidgetSystemNode::traverse(vsg::ConstVisitor& v) const
{
    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&v);

    if (ecsVisitor)
    {
        for (auto entity : _focusedEntities)
            ecsVisitor->collectedEntities.emplace(entity);
    }

    Inherit::traverse(v);
}

#endif // ROCKY_HAS_IMGUI
