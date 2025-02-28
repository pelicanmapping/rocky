
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Common.h>

#ifdef ROCKY_HAS_IMGUI
#include <imgui.h>
#include <rocky/vsg/ecs.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ViewLocal.h>

#include "WidgetSystem.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    // Internal component for rendering a widget instance
    struct WidgetRenderable
    {
        const std::string uid = std::to_string((std::uintptr_t)this);
        detail::ViewLocal<ImVec2> screen;
        ImVec2 windowSize = { 0, 0 };
    };

    void on_construct_Widget(entt::registry& r, entt::entity e)
    {
        r.emplace<ActiveState>(e);
        r.emplace<Visibility>(e);
        r.emplace<WidgetRenderable>(e);
    }
}


WidgetSystemNode::WidgetSystemNode(ecs::Registry& in_registry) :
    vsg::Inherit<vsg::Node, WidgetSystemNode>(),
    ecs::System(in_registry)
{
    // configure EnTT to automatically add the necessary components when a Widget is constructed
    auto [lock, registry] = _registry.write();
    registry.on_construct<Widget>().connect<&on_construct_Widget>();
}

WidgetSystemNode::~WidgetSystemNode()
{
    auto [lock, registry] = _registry.write();
    registry.on_construct<Widget>().disconnect<&on_construct_Widget>();
}

void
WidgetSystemNode::initialize(VSGContext& context)
{
    // register me as a gui rendering callback.
    auto render = [this](std::uint32_t viewID, void* imguiContext)
        {            
            auto [lock, registry] = _registry.read();

            ImGui::SetCurrentContext((ImGuiContext*)imguiContext);

            const int defaultWindowFlags =
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoSavedSettings;

            auto view = registry.view<Widget, WidgetRenderable, Transform, Visibility>();
            for (auto&& [entity, widget, renderable, transform, visibility] : view.each())
            {
                if (ecs::visible(visibility, viewID) && transform.node->visible(viewID))
                {
                    WidgetInstance i{
                        widget,
                        renderable.uid,
                        registry,
                        entity,
                        defaultWindowFlags,
                        renderable.screen[viewID],
                        renderable.windowSize,
                        (ImGuiContext*)imguiContext,
                        viewID
                    };

                    if (widget.render)
                    {
                        // Note: widget render needs to call ImGui::SetCurrentContext(i.context)
                        widget.render(i);
                    }

                    else
                    {
                        float x = renderable.screen[viewID].x - renderable.windowSize.x * 0.5f;
                        float y = renderable.screen[viewID].y - renderable.windowSize.y * 0.5f;

                        ImGui::SetNextWindowPos(ImVec2(x, y));

                        if (ImGui::Begin(i.uid.c_str(), nullptr, i.defaultWindowFlags))
                        {
                            ImGui::Text(widget.text.c_str());
                            i.size = ImGui::GetWindowSize();
                        }
                        ImGui::End();
                    }
                }
            }
        };

    context->guiRenderers.emplace_back(render);
}

void
WidgetSystemNode::update(VSGContext& context)
{
    auto [lock, registry] = _registry.read();

    // calculate the screen position of the widget in each view
    registry.view<WidgetRenderable, Transform>().each([&](auto& renderable, auto& transform)
        {
            vsg::dmat4 mvp;
            for(auto& viewID : context->activeViewIDs)
            {
                auto& local = transform.node->viewLocal[viewID];
                ROCKY_FAST_MAT4_MULT(mvp, local.proj, local.modelview);
                auto clip = mvp[3] / mvp[3][3];
                renderable.screen[viewID].x = (clip.x + 1.0) * 0.5 * (double)local.viewport[2];
                renderable.screen[viewID].y = (clip.y + 1.0) * 0.5 * (double)local.viewport[3];
            }
        });
}

#endif // ROCKY_HAS_IMGUI
