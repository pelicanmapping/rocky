/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/MapObject.h>

#include <vsg/app/Viewer.h>
#include <vsg/app/Window.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/app/CommandGraph.h>
#include <vsg/app/View.h>
#include <vsg/nodes/Group.h>
#include <vsg/commands/ClearAttachments.h>

#include <chrono>
#include <list>

namespace ROCKY_NAMESPACE
{
    class ROCKY_VSG_EXPORT Application
    {
    public:
        //! Construct a new application object
        //! @param argc Number of command line arguments, including the exe name
        //! @param argv Command line arguments
        Application(int& argc, char** argv);

        //! Run until exit.
        //! @return exit code, 0 = no error
        int run();

        //! Access the map.
        //! @return Pointer to the map
        shared_ptr<Map> map();

        //! Add a map object to the scene
        //! @param object Map object to add to the scene
        void add(shared_ptr<MapObject> object);

        //! Remove a map object from the scene
        //! @param object Map object to remove from the scene
        void remove(shared_ptr<MapObject> object);


    public: // Windows and Views

        //! Collection of windows and views managed by the application.
        struct DisplayConfiguration
        {
            using Windows = std::map<
                vsg::ref_ptr<vsg::Window>,
                std::list<vsg::ref_ptr<vsg::View>>>;

            Windows windows;
        };

        //! Information about each view.
        struct ViewData
        {
            vsg::ref_ptr<vsg::RenderGraph> parentRenderGraph;
        };

        //! Adds a window to the application. This may happen asynchronously
        //! depending on when you invoke it.
        //! 
        //! @param traits Window traits used to create the new window
        //! @return Pointer to the new window (when available)
        util::Future<vsg::ref_ptr<vsg::Window>> addWindow(
            vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a view to an existing window. This may happen asynchronously
        //! depending on when you invoke it.
        //! 
        //! @param window Window to which to add the new view
        //! @param view New view to add
        //! @param on_create Function to run after the view is added
        //! @return Pointer to the new view (when available)
        util::Future<vsg::ref_ptr<vsg::View>> addView(
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::View> view,
            std::function<void()> on_create = {});

        //! Removes a view from a window
        //! @param view View to remove from its window
        void removeView(vsg::ref_ptr<vsg::View> view);

        //! Refreshes a view after changing its parameters like viewport, clear color, etc.
        //! @param View view to refresh
        void refreshView(vsg::ref_ptr<vsg::View> view);

        //! Return commands and data pertaining to a view.
        //! @param view View for which to retrieve data
        //! @return Reference to the view's supplementary data
        inline ViewData& viewData(vsg::ref_ptr<vsg::View> view);

        //! Adds a child to a window's main render pass. Each render pass child works as 
        //! a "stage" and these stages render in their order of appearance. This is is
        //! good way to e.g. render a GUI overlay after rendering the 3D scene.
        //! @param window Window to which to add the post-render node.
        //! @param node Node to add.
        void addPostRenderNode(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::Node> node);

    public:
        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        std::function<void()> updateFunction;
        DisplayConfiguration displayConfiguration;

        struct Stats
        {
            std::chrono::microseconds frame;
            std::chrono::microseconds events;
            std::chrono::microseconds update;
            std::chrono::microseconds record;
        };
        Stats stats;

        //! About the application. Lists all the dependencies and their versions.
        std::string about() const;

    public:
        //! Copy construction is disabled.
        Application(const Application&) = delete;

        //! Move construction is disabled.
        Application(Application&&) = delete;

    private:
        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
        bool _multithreaded = true;
        bool _viewerRealized = false;
        bool _viewerDirty = false;
        AttachmentRenderers _renderers;

        struct Addition {
            vsg::ref_ptr<vsg::Node> node;
            vsg::CompileResult compileResult;
        };

        std::mutex _add_remove_mutex;
        std::list<util::Future<Addition>> _objectsToAdd;
        std::list<vsg::ref_ptr<vsg::Node>> _objectsToRemove;
        std::map<vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>> _commandGraphByWindow;
        
        std::map<vsg::ref_ptr<vsg::View>, ViewData> _viewData;
        
        void realizeViewer(vsg::ref_ptr<vsg::Viewer> viewer);

        void recreateViewer();

        void addAndRemoveObjects();

        void addViewAfterViewerIsRealized(
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::View> view,
            std::function<void()> on_create,
            util::Future<vsg::ref_ptr<vsg::View>> result);

        void addManipulator(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View>);
    };

    // inlines.
    Application::ViewData& Application::viewData(vsg::ref_ptr<vsg::View> view) {
        return _viewData[view];
    }
}
