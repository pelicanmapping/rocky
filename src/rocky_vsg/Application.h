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
#include <vsg/nodes/Group.h>
#include <vsg/text/Text.h>

#include <list>

namespace ROCKY_NAMESPACE
{
    class ROCKY_VSG_EXPORT Application
    {
    public:
        //! Construct a new application object
        Application(int& argc, char** argv);

        //! Create a main window.
        void createMainWindow(int width, int height, const std::string& name = {});

        //! Access the map.
        shared_ptr<Map> map();

        //! Run until exit.
        int run();

        //! Add a map object to the scene
        void add(shared_ptr<MapObject> object);

        //! Remove a map object from the scene
        void remove(shared_ptr<MapObject> object);

    public:

        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Window> mainWindow;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        std::function<void()> updateFunction;

    public:
        Application(const Application&) = delete;
        Application(Application&&) = delete;

    protected:
        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
        AttachmentRenderers _renderers;

        struct Addition {
            vsg::ref_ptr<vsg::Node> node;
            vsg::CompileResult compileResult;
        };

        std::mutex _add_remove_mutex;
        std::list<util::Future<Addition>> _additions;
        std::list<vsg::ref_ptr<vsg::Node>> _removals;

        void processAdditionsAndRemovals();
    };
}
