/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <vsg/app/Viewer.h>
#include <vsg/app/Window.h>
#include <vsg/nodes/Group.h>

namespace ROCKY_NAMESPACE
{
    class ROCKY_VSG_EXPORT EngineVSG
    {
    public:
        //! Construct a new engine.
        EngineVSG(int& argc, char** argv);

        //! Create a main window.
        void createMainWindow(int width, int height, const std::string& name = {});

        //! Access the map.
        shared_ptr<Map> map();

        //! Run until exit.
        int run();

    public:

        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Window> mainWindow;
        vsg::ref_ptr<vsg::Group> mainScene;

    protected:
        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
    };
}
