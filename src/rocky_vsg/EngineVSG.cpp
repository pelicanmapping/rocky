/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "EngineVSG.h"

#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/TerrainNode.h>
#include <rocky_vsg/MapManipulator.h>
#include <rocky/Ephemeris.h>

#include <vsg/app/CloseHandler.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/nodes/Light.h>

using namespace ROCKY_NAMESPACE;


EngineVSG::EngineVSG(int& argc, char** argv) :
    instance()
{
    vsg::CommandLine commandLine(&argc, argv);
    
    commandLine.read(instance._impl->vsgOptions);
    _debuglayer = commandLine.read({ "--debug" });
    _apilayer = commandLine.read({ "--api" });
    _vsync = !commandLine.read({ "--novsync" });

    viewer = vsg::Viewer::create();
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    mapNode = rocky::MapNode::create(instance);

    mapNode->terrainNode()->concurrency = 4u;
    mapNode->terrainNode()->skirtRatio = 0.05f;
    mapNode->terrainNode()->minLevelOfDetail = 1;
    mapNode->terrainNode()->screenSpaceError = 135.0f;

    mainScene = vsg::Group::create();
    mainScene->addChild(mapNode);

    // the sun
    if (commandLine.read({ "--sky" }))
    {
        auto sun = rocky::Ephemeris().sunPosition(rocky::DateTime());
        auto light = vsg::PointLight::create();
        light->name = "Sol";
        light->color = { 1, 1, 0.95 };
        light->position = { sun.geocentric.x, sun.geocentric.y, sun.geocentric.z };
        mainScene->addChild(light);
    }
}

void
EngineVSG::createMainWindow(int width, int height, const std::string& name)
{
    auto traits = vsg::WindowTraits::create(name);
    traits->debugLayer = _debuglayer;
    traits->apiDumpLayer = _apilayer;
    traits->samples = 1;
    traits->width = 1920;
    traits->height = 1080;
    if (!_vsync)
        traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    mainWindow = vsg::Window::create(traits);
    mainWindow->clearColor() = VkClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f };
    viewer->addWindow(mainWindow);
}

std::shared_ptr<Map>
EngineVSG::map()
{
    return mapNode->map();
}

int
EngineVSG::run()
{
    // Make a window if the user didn't.
    if (!mainWindow)
    {
        createMainWindow(1920, 1080);
    }

    // Configure out mapnode to our liking:
    mapNode->terrainNode()->concurrency = 4u;
    mapNode->terrainNode()->skirtRatio = 0.1f;
    mapNode->terrainNode()->minLevelOfDetail = 1;
    mapNode->terrainNode()->screenSpaceError = 135.0f;

    // Set up the runtime context with everything we need.
    // Eventually this should be automatic in InstanceVSG
    instance.runtime().compiler = [this]() { return viewer->compileManager; };
    instance.runtime().updates = [this]() { return viewer->updateOperations; };
    instance.runtime().sharedObjects = vsg::SharedObjects::create();

    // main camera
    double nearFarRatio = 0.00001;
    double R = mapNode->mapSRS().ellipsoid().semiMajorAxis();

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(mainWindow->extent2D().width) / (double)(mainWindow->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto camera = vsg::Camera::create(
        perspective,
        vsg::LookAt::create(),
        vsg::ViewportState::create(mainWindow->extent2D()));

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, camera));

    // associate the scene graph with a window and camera in a new render graph
    auto renderGraph = vsg::createRenderGraphForView(
        mainWindow,
        camera,
        mainScene,
        VK_SUBPASS_CONTENTS_INLINE,
        false); // assignHeadlight

    // Command graph holds the render graph:
    auto commandGraph = vsg::CommandGraph::create(mainWindow);
    commandGraph->addChild(renderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

    // Configure a descriptor pool size that's appropriate for paged terrains
    // (they are a good candidate for DS reuse)
    // https://groups.google.com/g/vsg-users/c/JJQZ-RN7jC0/m/tyX8nT39BAAJ
    auto resourceHints = vsg::ResourceHints::create();
    resourceHints->numDescriptorSets = 1024;
    resourceHints->descriptorPoolSizes.push_back(
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 });

    // configure the viewers rendering backend, initialize and compile Vulkan objects,
    // passing in ResourceHints to guide the resources allocated.
    viewer->compile(resourceHints);

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();

        // since an event handler could deactivate the viewer:
        if (!viewer->active())
            break;

        // rocky update pass - management of tiles and paged data
        mapNode->update(viewer->getFrameStamp());

        // runs through the viewer's update operations queue; this includes update ops 
        // initialized by rocky (tile merges for example)
        viewer->update();

        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}
