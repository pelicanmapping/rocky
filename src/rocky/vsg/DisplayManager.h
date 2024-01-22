/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/InstanceVSG.h>
#include <rocky/vsg/MapManipulator.h>

#include <vsg/app/Viewer.h>
#include <vsg/app/Window.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/app/CommandGraph.h>
#include <vsg/app/View.h>
#include <vsg/nodes/Group.h>

#include <chrono>
#include <list>

namespace ROCKY_NAMESPACE
{
    class Application;

    class ROCKY_EXPORT DisplayManager
    {
    public:
        DisplayManager(Application& in_app);

        //! Information about each view.
        struct ViewData
        {
            vsg::ref_ptr<vsg::RenderGraph> parentRenderGraph;
        };

        //! Adds a pre-existing window to the display.
        //! @param window Window to add
        void addWindow(vsg::ref_ptr<vsg::Window> window);

        //! Adds a new window to the display.
        //! @param traits Window traits used to create the new window
        vsg::ref_ptr<vsg::Window> addWindow(vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a view to an existing window. This may happen asynchronously
        //! depending on when you invoke it.
        //! 
        //! @param window Window to which to add the new view
        //! @param view New view to add
        //! @param on_create Function to run after the view is added
        //! @return Pointer to the new view (when available)
        void addViewToWindow(
            vsg::ref_ptr<vsg::View> view,
            vsg::ref_ptr<vsg::Window> window,
            std::function<void(vsg::CommandGraph*)> on_create = {});

        //! Removes a view from a window
        //! @param view View to remove from its window
        void removeView(vsg::ref_ptr<vsg::View> view);

        //! Refreshes a view after changing its parameters like viewport, clear color, etc.
        //! @param View view to refresh
        void refreshView(vsg::ref_ptr<vsg::View> view);

        //! Access additional information pertaining to a View
        //! @param view View for which to retrieve data
        //! @return Reference to the view's supplementary data
        //! TODO: evaluate whether we need this function or whether it should be exposed
        ViewData& viewData(vsg::ref_ptr<vsg::View> view);

        //! TODO:
        //! REFACTOR or REPLACE this based on the new functionality described here:
        //! https://github.com/vsg-dev/VulkanSceneGraph/discussions/928
        //! 
        //! Adds a new render graph that should render before the rest of the scene.
        //! (Typical use is an RTT camera that is later used as a texture somewhere
        //! in the main scene graph.)
        //! @param window Window under which to add the new graph
        //! @param renderGraph RenderGraph to add
        //! TODO: add a way to remove or deactivate it
        void addPreRenderGraph(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::RenderGraph> renderGraph);

        //! Adds a stock map manipulator to a view.
        void addManipulator(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View>);

        //! Gets the map manipulator associated with a view, or nullptr if non exists.
        vsg::ref_ptr<MapManipulator> getMapManipulator(vsg::ref_ptr<vsg::View> view);

        //! Gets the VSG command graph associated with the provided window.
        vsg::ref_ptr<vsg::CommandGraph> getCommandGraph(vsg::ref_ptr<vsg::Window> window);

        //! Gets the window hosting the provided view.
        vsg::ref_ptr<vsg::Window> getWindow(vsg::ref_ptr<vsg::View> view);

    public:
        Application& app;

        using Windows = std::map<
            vsg::ref_ptr<vsg::Window>,
            std::list<vsg::ref_ptr<vsg::View>>>;

        Windows windows;

    protected:
        std::map<vsg::ref_ptr<vsg::View>, ViewData> _viewData;
        bool _debugCallbackInstalled = false;
        std::map<vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>> _commandGraphByWindow;
        friend class Application;
    };
}
