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

#include <list>

namespace ROCKY_NAMESPACE
{
    class ROCKY_VSG_EXPORT Application
    {
    public:
        //! Construct a new application object
        Application(int& argc, char** argv);

        //! Run until exit.
        int run();

        //! Access the map.
        shared_ptr<Map> map();

        //! Add a map object to the scene
        void add(shared_ptr<MapObject> object);

        //! Remove a map object from the scene
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

        //! Adds a window to the application.
        util::Future<vsg::ref_ptr<vsg::Window>> addWindow(vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a view to an existing window
        util::Future<vsg::ref_ptr<vsg::View>> addView(
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::View> view,
            std::function<void()> on_create = {});

        //! Removes a view from a window
        void removeView(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view);

        //! Adds a child to a window's main render pass. Each render pass child works as 
        //! a "stage" and these stages render in their order of appearance. This is is
        //! good way to e.g. render a GUI overlay after rendering the 3D scnee.
        void addPostRenderNode(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::Node> node);

        //! Refreshes a view after changing its parameters like viewport, clear color, etc.
        void refreshView(vsg::ref_ptr<vsg::View> view);

        //! Return commands and data pertaining to a view.
        inline ViewData& viewData(vsg::ref_ptr<vsg::View> view);

    public:
        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        std::function<void()> updateFunction;
        DisplayConfiguration displayConfiguration;


        //! About the application.
        std::string about() const;

    public:
        Application(const Application&) = delete;
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
