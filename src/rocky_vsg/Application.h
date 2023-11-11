/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/SkyNode.h>
#include <rocky_vsg/ECS.h>

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

        //! Process and render one frame. If you call run(), this will
        //! happen automatically in a continuous loop.
        //! @return True upon success; false to quit the application.
        bool frame();

    public: // public properties

        entt::registry entities;
        ECS::SystemsManager ecs;

        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<rocky::SkyNode> skyNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        vsg::ref_ptr<ECS::VSG_SystemsGroup> ecs_node;
        std::function<void()> updateFunction;

        //! Collection of windows and views managed by the application.
        struct DisplayConfiguration
        {
            using Windows = std::map<
                vsg::ref_ptr<vsg::Window>,
                std::list<vsg::ref_ptr<vsg::View>>>;

            Windows windows;
        };
        DisplayConfiguration displayConfiguration;

        //! Runtime timing statistics
        struct Stats
        {
            std::chrono::microseconds frame;
            std::chrono::microseconds events;
            std::chrono::microseconds update;
            std::chrono::microseconds record;
            std::chrono::microseconds present;
            double memory;

        };
        Stats stats;

        //! About the application. Lists all the dependencies and their versions.
        std::string about() const;

        //! True if the debug validation layer is active, which will affect performance
        bool debugLayerOn() const {
            return _debuglayer;
        }

    public: // Windows and Views

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
        inline ViewData& viewData(vsg::ref_ptr<vsg::View> view);

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


    public:
        //! Copy construction is disabled.
        Application(const Application&) = delete;

        //! Move construction is disabled.
        Application(Application&&) = delete;

        //! Destructor
        ~Application();

    private:
        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
        bool _multithreaded = true;
        bool _viewerRealized = false;
        bool _viewerDirty = false;

        void realize();

        std::map<vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>> _commandGraphByWindow;
        
        std::map<vsg::ref_ptr<vsg::View>, ViewData> _viewData;
        
        void setupViewer(vsg::ref_ptr<vsg::Viewer> viewer);

        void recreateViewer();

        void addViewAfterViewerIsRealized(
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::View> view,
            std::function<void(vsg::CommandGraph*)> on_create,
            util::Future<vsg::ref_ptr<vsg::View>> result);

        vsg::ref_ptr<vsg::CommandGraph> getCommandGraph(vsg::ref_ptr<vsg::Window> window);
        vsg::ref_ptr<vsg::Window> getWindow(vsg::ref_ptr<vsg::View> view);

        void addManipulator(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View>);
    };

    // inlines.
    Application::ViewData& Application::viewData(vsg::ref_ptr<vsg::View> view) {
        return _viewData[view];
    }
}

