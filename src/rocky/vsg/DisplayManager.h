/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/Utils.h>
#include <rocky/Callbacks.h>

#include <list>

namespace ROCKY_NAMESPACE
{
    class Application;
    class ImGuiContextGroup;

    //! Return value for pointAtWindowCoords(viewer) method
    struct DisplayGeoPoint
    {
        vsg::ref_ptr<vsg::Window> window;
        vsg::ref_ptr<vsg::View> view;
        GeoPoint point;
    };

    //! Return the GeoPoint at the given window coordinates (e.g., mouse position).
    //! @param window View in which to search
    //! @param x X coordinate in window space
    //! @param y Y coordinate in window space
    //! @return GeoPoint at the given window coordinates
    extern ROCKY_EXPORT GeoPoint pointAtWindowCoords(vsg::ref_ptr<vsg::View> view, int x, int y);

    //! Return the GeoPoint at the given window coordinates (e.g., mouse position).
    //! @param viewer Viewer to search for windows and views
    //! @param x X coordinate in window space
    //! @param y Y coordinate in window space
    //! @return GeoPoint at the given window coordinates
    extern ROCKY_EXPORT DisplayGeoPoint pointAtWindowCoords(vsg::ref_ptr<vsg::Viewer> viewer, int x, int y);

    /**
    * DisplayManager is a helper class that manages the creation and destruction of
    * windows and views. It also provides some utility methods for working with
    * render graphcs an manipulators.
    */
    class ROCKY_EXPORT DisplayManager
    {
    public:
        //! Construct an empty display manager.
        //! Call initialize() later to set it up.
        DisplayManager() = default;

        //! Construct a display manager connected to an Application object.
        void initialize(Application& app);

        //! Construct a display manager connected to a VSG context (and associated viewer).
        //! Use this if your app doesn't use the rocky Application object.
        void initialize(VSGContext context);

        //! Creates a new window and adds it to the display.
        //! @param traits Window traits used to create the new window
        vsg::ref_ptr<vsg::Window> addWindow(vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a pre-existing window to the display.
        //! @param window Window to add
        //! @param view View to use for the window (optional - if not provided, a default view will be created)
        void addWindow(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view = {});

        //! Removes a window from the display.
        //! #param window Window to remove
        void removeWindow(vsg::ref_ptr<vsg::Window> window);

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

        //! Gets the view containing the window coordinates (e.g., mouse position relative to the window)
        //! @param window Window to query
        //! @param x X coordinate in window space
        //! @param y Y coordinate in window space
        //! @return View containing the coordinates, or nullptr.
        vsg::ref_ptr<vsg::View> getView(vsg::ref_ptr<vsg::Window> window, double x, double y);

        //! Gets the VSG command graph associated with a window.
        vsg::ref_ptr<vsg::CommandGraph> getCommandGraph(vsg::ref_ptr<vsg::Window> window);

        //! Gets the VSG render graph associated with a view.
        vsg::ref_ptr<vsg::RenderGraph> getRenderGraph(vsg::ref_ptr<vsg::View> view);

        //! Gets the window hosting the provided view.
        vsg::ref_ptr<vsg::Window> getWindow(vsg::ref_ptr<vsg::View> view);

        //! Gets the vulkan device shared by all windows
        vsg::ref_ptr<vsg::Device> sharedDevice();

        //! Compile and hook up a render graph that you have manually installed
        //! on a command graph.
        void compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph> renderGraph, vsg::ref_ptr<vsg::Window> window);

    public:

        using WindowsAndViews = std::map<vsg::ref_ptr<vsg::Window>, std::list<vsg::ref_ptr<vsg::View>>>;

        WindowsAndViews windowsAndViews;

        VSGContext vsgcontext;

    protected:
        struct ViewData
        {
            vsg::ref_ptr<vsg::RenderGraph> parentRenderGraph;
            vsg::ref_ptr<vsg::Group> guiContextGroup;
            vsg::ref_ptr<vsg::Visitor> guiEventVisitor;
            std::shared_ptr<std::function<void()>> guiIdleEventProcessor;
        };
        util::vector_map<vsg::ref_ptr<vsg::View>, ViewData> _viewData;

        Application* _app = nullptr;
        bool _debugCallbackInstalled = false;
        std::map<vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>> _commandGraphByWindow;

        friend class Application;
        friend struct ImGuiIntegration;
    };
}
