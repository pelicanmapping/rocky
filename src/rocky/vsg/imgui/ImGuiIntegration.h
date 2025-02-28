/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/imgui/SendEventsToImGui.h>
#include <rocky/vsg/imgui/RenderImGui.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Node that lives under a RenderImGui node, and invokes any GUI renderers
    * installed on the VSGContext.
    */
    class GuiRendererDispatcher : public vsg::Inherit<vsg::Node, GuiRendererDispatcher>
    {
    public:
        VSGContext context;

        GuiRendererDispatcher(VSGContext context_in) : context(context_in) { }

        void traverse(vsg::RecordTraversal& record) const override
        {
            for (auto& callback : context->guiRenderers)
            {
                callback(record.getState()->_commandBuffer->viewID, ImGui::GetCurrentContext());
            }
        }
    };

    //! wrapper for vsgImGui::SendEventsToImGui that restricts ImGui events to a single window & imgui context,
    //! of which there needs to be one per view.
    class ROCKY_EXPORT SendEventsToImGuiWrapper : public vsg::Inherit<vsgImGui::SendEventsToImGui, SendEventsToImGuiWrapper>
    {
    public:
        SendEventsToImGuiWrapper(vsg::ref_ptr<vsg::Window> window, ImGuiContext* imguiContext, VSGContext& vsgContext) :
            _window(window), _imguiContext(imguiContext), _vsgContext(vsgContext)
        {
            //nop
        }

        template<typename E>
        inline void propagate(E& e, bool forceRefresh = false)
        {
            // only process events for the window we are interested in, and if the event wasn't handled
            // (say, by another wrapper connected to another view)
            if (!e.handled && e.window.ref_ptr() == _window)
            {
                // active the context associated with this window/view
                ImGui::SetCurrentContext(_imguiContext);

                vsgImGui::SendEventsToImGui::apply(e);

                if (e.handled || forceRefresh)
                {
                    _vsgContext->requestFrame();
                }
            }
        }

        void apply(vsg::ButtonPressEvent& e) override { propagate(e); }
        void apply(vsg::ButtonReleaseEvent& e) override { propagate(e); }
        void apply(vsg::ScrollWheelEvent& e) override { propagate(e); }
        void apply(vsg::KeyPressEvent& e) override { propagate(e); }
        void apply(vsg::KeyReleaseEvent& e) override { propagate(e); }
        void apply(vsg::MoveEvent& e) override { propagate(e); }
        void apply(vsg::ConfigureWindowEvent& e) override { propagate(e, true); }
        
        void apply(vsg::FrameEvent& frame) override {
            ImGui::SetCurrentContext(_imguiContext);
            Inherit::apply(frame);
        }

    private:
        vsg::ref_ptr<vsg::Window> _window;
        VSGContext _vsgContext;
        ImGuiContext* _imguiContext;
    };
}
