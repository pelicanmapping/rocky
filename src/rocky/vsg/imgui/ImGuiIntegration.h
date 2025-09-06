/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/Callbacks.h>
#include <rocky/vsg/imgui/SendEventsToImGui.h>
#include <rocky/vsg/imgui/RenderImGui.h>

#ifdef ROCKY_HAS_IMGUI
#include <imgui.h>

/**
* Classes here are extensions of the vsgImGui classes to support multi-context
* ImGui rendering. This is neccessary when crossing DLL boundaries and the like.
*/

namespace ROCKY_NAMESPACE
{
    class Application;
    class DisplayManager;

    //! wrapper for vsgImGui::SendEventsToImGui that restricts ImGui events to a single window & imgui context,
    //! of which there needs to be one per view.
    class ROCKY_EXPORT SendEventsToImGuiContext : public vsg::Inherit<SendEventsToImGui, SendEventsToImGuiContext>
    {
    public:
        SendEventsToImGuiContext(vsg::ref_ptr<vsg::Window> window, ImGuiContext* imguiContext) :
            _window(window), _imguiContext(imguiContext)
        {
            //nop
        }

        Callback<void(const vsg::UIEvent&)> onEvent;

        template<typename E>
        inline void propagate(E& e)
        {
            // only process events for the window we are interested in, and if the event wasn't handled
            // (say, by another wrapper connected to another view)
            if (!e.handled && ((_window == nullptr) || (e.window.ref_ptr() == _window)))
            {
                // activate the context associated with this window/view
                if (_imguiContext)
                {
                    ImGui::SetCurrentContext(_imguiContext);
                }

                Inherit::apply(e);

                onEvent.fire(e);
            }
        }

        void apply(vsg::ButtonPressEvent& e) override { propagate(e); }
        void apply(vsg::ButtonReleaseEvent& e) override { propagate(e); }
        void apply(vsg::ScrollWheelEvent& e) override { propagate(e); }
        void apply(vsg::KeyPressEvent& e) override { propagate(e); }
        void apply(vsg::KeyReleaseEvent& e) override { propagate(e); }
        void apply(vsg::MoveEvent& e) override { propagate(e); }
        void apply(vsg::ConfigureWindowEvent& e) override { propagate(e); }

        void apply(vsg::FrameEvent& e) override {
            if (_imguiContext)
                ImGui::SetCurrentContext(_imguiContext);
            Inherit::apply(e);
            onEvent.fire(e);
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
    class ROCKY_EXPORT ImGuiContextNode : public vsg::Inherit<vsg::Node, ImGuiContextNode>
    {
    public:
        ImGuiContextNode() = default;

        //! Installs the context pointer on the record traversal.
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
    class ROCKY_EXPORT RenderImGuiContext : public vsg::Inherit<RenderImGui, RenderImGuiContext>
    {
    public:
        //! Window ImGui will render to
        vsg::ref_ptr<vsg::Window> window;
        
        //! View ImGui will render to, or null for the first view
        vsg::ref_ptr<vsg::View> view;

        //! Fired when user adds a node
        Callback<void(vsg::ref_ptr<ImGuiContextNode>)> onNodeAdded;

        //! whether to enable docking, if supported by ImGui
        bool enableDocking = false;

        //! Construct a new ImGui renderer
        RenderImGuiContext(vsg::ref_ptr<vsg::Window> in_window, vsg::ref_ptr<vsg::View> in_view = {}) :
            Inherit(in_window), window(in_window), view(in_view)
        {
            //nop
        }

        //! Add a Gui Node to this renderer
        void add(vsg::ref_ptr<ImGuiContextNode> node);

    public:

        void traverse(vsg::RecordTraversal& record) const override;

    protected:
        mutable bool _firstFrame = true;
    };

    namespace detail
    {
        /**
        * Node that lives under a RenderImGui node, and invokes any GUI renderers
        * installed on the VSGContext (for example, the one used by the WidgetSystem).
        */
        class ImGuiDispatcher : public vsg::Inherit<vsg::Node, ImGuiDispatcher>
        {
        public:
            ImGuiContext* imguicontext = nullptr;
            VSGContext vsgcontext;

            ImGuiDispatcher(ImGuiContext* imguiContext_in, VSGContext vsgContext_in)
                : imguicontext(imguiContext_in), vsgcontext(vsgContext_in) {
            }

            void traverse(vsg::RecordTraversal& record) const override
            {
                detail::RenderingState rs{
                    record.getCommandBuffer()->viewID,
                    record.getFrameStamp()->frameCount
                };

                for (auto& record_gui : vsgcontext->guiRecorders)
                {
                    record_gui(rs, imguicontext);
                }
            }
        };
    }
}

#endif // ROCKY_HAS_IMGUI
