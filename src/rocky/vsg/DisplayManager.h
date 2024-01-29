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

    /**
    * DisplayManager is a helper class that manages the creation and destruction of
    * windows and views. It also provides some utility methods for working with
    * render graphcs an manipulators.
    */
    class ROCKY_EXPORT DisplayManager
    {
    public:
        //! Construct a display manager that is connected to a rocky Application object.
        DisplayManager(Application& in_app);

        //! Construct a display manager connected to a user-created Viewer.
        //! Use this if your app doesn't use the rocky Application object.
        DisplayManager(vsg::ref_ptr<vsg::Viewer> viewer);

        //! Creates a new window and adds it to the display.
        //! @param traits Window traits used to create the new window
        vsg::ref_ptr<vsg::Window> addWindow(vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a pre-existing window to the display.
        //! @param window Window to add
        //! @param view View to use for the window (optional - if not provided, a default view will be created)
        void addWindow(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view = {});

        //! Adds a view to an existing window.
        //! @param view New view to add
        //! @param window Window to which to add the new view
        void addViewToWindow(vsg::ref_ptr<vsg::View> view, vsg::ref_ptr<vsg::Window> window);

        //! Removes a view from its host window.
        //! @param view View to remove from its window
        void removeView(vsg::ref_ptr<vsg::View> view);

        //! Refreshes a view after changing its parameters like viewport, clear color, etc.
        //! @param View view to refresh
        void refreshView(vsg::ref_ptr<vsg::View> view);

        //! Adds a manipulator to a view.
        //! @param manip Manipulator to set
        //! @view View on which to set the manipulator
        void setManipulatorForView(vsg::ref_ptr<MapManipulator> manip, vsg::ref_ptr<vsg::View> view);

        //! Gets the VSG command graph associated with a window.
        vsg::ref_ptr<vsg::CommandGraph> getCommandGraph(vsg::ref_ptr<vsg::Window> window);

        //! Gets the VSG render graph associated with a view.
        vsg::ref_ptr<vsg::RenderGraph> getRenderGraph(vsg::ref_ptr<vsg::View> view);

        //! Gets the window hosting the provided view.
        vsg::ref_ptr<vsg::Window> getWindow(vsg::ref_ptr<vsg::View> view);

        //! Compile and hook up a render graph that you have manually installed
        //! on a command graph.
        void compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph> renderGraph, vsg::ref_ptr<vsg::Window> window);

    public:

        using WindowsAndViews = std::map<vsg::ref_ptr<vsg::Window>, std::list<vsg::ref_ptr<vsg::View>>>;

        WindowsAndViews windowsAndViews;

        vsg::ref_ptr<vsg::Viewer> viewer;

    protected:
        Application* app = nullptr;

        struct ViewData
        {
            vsg::ref_ptr<vsg::RenderGraph> parentRenderGraph;
        };
        std::map<vsg::ref_ptr<vsg::View>, ViewData> _viewData;

        bool _debugCallbackInstalled = false;
        std::map<vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>> _commandGraphByWindow;
        friend class Application;
    };
}
