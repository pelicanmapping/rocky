/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/imgui/SendEventsToImGui.h>
#include <rocky/vsg/imgui/RenderImGui.h>
#include <imgui.h>

namespace ROCKY_NAMESPACE
{
    class Application;
    class DisplayManager;

    namespace detail
    {
        /**
        * Node that lives under a RenderImGui node, and invokes any GUI renderers
        * installed on the VSGContext.
        */
        class GuiRendererDispatcher : public vsg::Inherit<vsg::Node, GuiRendererDispatcher>
        {
        public:
            ImGuiContext* imguiContext;
            VSGContext vsgContext;

            GuiRendererDispatcher(ImGuiContext* imguiContext_in, VSGContext vsgContext_in)
                : imguiContext(imguiContext_in), vsgContext(vsgContext_in) { }

            void traverse(vsg::RecordTraversal& record) const override
            {
                ViewRecordingState vrs{
                    record.getCommandBuffer()->viewID,
                    record.getFrameStamp()->frameCount
                };

                for (auto& record_gui : vsgContext->guiRecorders)
                {
                    record_gui(vrs, imguiContext);
                }
            }
        };
    }

    //! wrapper for vsgImGui::SendEventsToImGui that restricts ImGui events to a single window & imgui context,
    //! of which there needs to be one per view.
    class ROCKY_EXPORT SendEventsToImGuiWrapper : public vsg::Inherit<SendEventsToImGui, SendEventsToImGuiWrapper>
    {
    public:
        SendEventsToImGuiWrapper(vsg::ref_ptr<vsg::Window> window, ImGuiContext* imguiContext, VSGContext vsgContext = {}) :
            _window(window), _imguiContext(imguiContext), _vsgContext(vsgContext)
        {
            //nop
        }

        template<typename E>
        inline void propagate(E& e, bool forceRefresh = false)
        {
            // only process events for the window we are interested in, and if the event wasn't handled
            // (say, by another wrapper connected to another view)
            if (!e.handled && ((_window == nullptr) || (e.window.ref_ptr() == _window)))
            {
                // active the context associated with this window/view
                if (_imguiContext)
                {
                    ImGui::SetCurrentContext(_imguiContext);
                }

                SendEventsToImGui::apply(e);

                if (_vsgContext && (e.handled || forceRefresh))
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
        
        void apply(vsg::FrameEvent& frame) override
        {
            if (_imguiContext)
                ImGui::SetCurrentContext(_imguiContext);

            Inherit::apply(frame);
        }

    private:
        vsg::ref_ptr<vsg::Window> _window;
        VSGContext _vsgContext;
        ImGuiContext* _imguiContext;
    };

    /**
    * Node that renders ImGui commands.
    * This MUST live under a ImGuiContextGroup in order to work properly.
    */
    class ROCKY_EXPORT ImGuiNode : public vsg::Inherit<vsg::Node, ImGuiNode>
    {
    public:
        ImGuiNode() = default;

        void traverse(vsg::RecordTraversal& record) const override
        {
            ImGuiContext* imguiContext = nullptr;
            record.getValue("imgui.context", imguiContext);

            render(imguiContext);
        }

        virtual void render(ImGuiContext*) const = 0;
    };

    /**
    * Parent class for ImGuiNode's that has Application integration and represents
    * each ImGuiNode child in a separate ImGuiContext.
    */
    class ROCKY_EXPORT ImGuiContextGroup : public vsg::Inherit<RenderImGui, ImGuiContextGroup>
    {
    public:
        ImGuiContextGroup(const vsg::ref_ptr<vsg::Window>& window) :
            Inherit(window)
        {
            //nop
        }
        
        void add(vsg::ref_ptr<ImGuiNode> node, Application& app);
    };

    struct ROCKY_EXPORT ImGuiIntegration
    {
        //! Insalls an ImGuiContextGroup in a DisplayManager.
        static vsg::ref_ptr<ImGuiContextGroup> addContextGroup(
            DisplayManager* displayManager,
            vsg::ref_ptr<vsg::Window> window = {}, // default to first window
            vsg::ref_ptr<vsg::View> view = {}); // default to first view

        //! Insalls an ImGuiContextGroup in a DisplayManager.
        static vsg::ref_ptr<ImGuiContextGroup> addContextGroup(
            std::shared_ptr<DisplayManager> displayManager,
            vsg::ref_ptr<vsg::Window> window = {}, // default to first window
            vsg::ref_ptr<vsg::View> view = {}); // default to first view

        //! Installs an ImGui manager. You can add ImGuiNodes to the returned group
        //! to render them.
        static vsg::ref_ptr<ImGuiContextGroup> addContextGroup(
            vsg::ref_ptr<vsg::Viewer> viewer,
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::RenderGraph> renderGraph,
            VSGContext vsgContext = {});
    };
}
