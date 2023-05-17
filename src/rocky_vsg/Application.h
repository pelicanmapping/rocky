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

        struct DisplayConfiguration
        {
            using Windows = std::map<
                vsg::ref_ptr<vsg::Window>,
                std::list<vsg::ref_ptr<vsg::View>>>;

            Windows windows;
        };

        //! Adds a window to the application.
        util::Future<vsg::ref_ptr<vsg::Window>> addWindow(vsg::ref_ptr<vsg::WindowTraits> traits);

        //! Adds a view to an existing window
        void addView(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view);

        //! Removes a view from a window
        void removeView(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view);

        //! Adds a child to a window's main render pass. Each render pass child works as 
        //! a "stage" and these stages render in their order of appearance. This is is
        //! good way to e.g. render a GUI overlay after rendering the 3D scnee.
        void addPostRenderNode(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::Node> node);

    public:
        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        std::function<void()> updateFunction;
        DisplayConfiguration displayConfiguration;


        //! Find the render pass for a view:
        //! TODO: replace with a findRenderGraphForView() utility function.
        inline vsg::ref_ptr<vsg::RenderGraph> renderGraph(vsg::ref_ptr<vsg::View> view) const;

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
        std::map<vsg::ref_ptr<vsg::View>, vsg::ref_ptr<vsg::RenderGraph>> _renderGraphByView;
        
        void realizeViewer(vsg::ref_ptr<vsg::Viewer> viewer);

        void recreateViewer();

        void addAndRemoveObjects();

        void addViewAfterViewerIsRealized(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view);

        void addManipulator(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View>);
    };

    // inlines.
    vsg::ref_ptr<vsg::RenderGraph> Application::renderGraph(vsg::ref_ptr<vsg::View>view) const {
        auto i = _renderGraphByView.find(view);
        return i != _renderGraphByView.end() ? i->second : vsg::ref_ptr<vsg::RenderGraph>();
    }
}
