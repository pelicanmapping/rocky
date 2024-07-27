/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Application.h"
#include "MapNode.h"
#include "MapManipulator.h"
#include "SkyNode.h"
#include "json.h"
#include "engine/MeshSystem.h"
#include "engine/LineSystem.h"
#include "engine/IconSystem.h"
#include "engine/LabelSystem.h"

#include <rocky/contrib/EarthFileImporter.h>

#include <vsg/app/CloseHandler.h>
#include <vsg/app/View.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/vk/State.h>
#include <vsg/io/read.h>

using namespace ROCKY_NAMESPACE;

Application::Application()
{
    int argc = 0;
    char* argv[1] = { "rocky" };
    ctor(argc, argv);
}


Application::Application(int& argc, char** argv)
{
    ctor(argc, argv);
}

void
Application::ctor(int& argc, char** argv)
{
    vsg::CommandLine commandLine(&argc, argv);

    commandLine.read(instance._impl->runtime.readerWriterOptions);
    _debuglayer = commandLine.read({ "--debug" });
    _apilayer = commandLine.read({ "--api" });
    _vsync = !commandLine.read({ "--novsync" });
    //_multithreaded = commandLine.read({ "--mt" });

    if (commandLine.read({ "--version" }))
    {
        std::cout << "rocky " << ROCKY_VERSION_STRING << std::endl;
        exit(0);
    }

    if (commandLine.read({ "--version-all" }))
    {
        std::cout << about() << std::endl;
        exit(0);
    }

    root = vsg::Group::create();

    mainScene = vsg::Group::create();

    root->addChild(mainScene);

    mapNode = rocky::MapNode::create(instance);

    // the sun
    if (commandLine.read({ "--sky" }))
    {
        skyNode = rocky::SkyNode::create(instance);
        mainScene->addChild(skyNode);
    }

    mapNode->terrainSettings().concurrency = 6u;
    mapNode->terrainSettings().skirtRatio = 0.025f;
    mapNode->terrainSettings().minLevelOfDetail = 1;
    mapNode->terrainSettings().screenSpaceError = 135.0f;

    // wireframe overlay
    if (commandLine.read({ "--wire" }))
        instance.runtime().shaderCompileSettings->defines.insert("RK_WIREFRAME_OVERLAY");

    // a node to render the map/terrain
    mainScene->addChild(mapNode);

    // Set up the runtime context with everything we need.
    setViewer(vsg::Viewer::create());

    instance.runtime().sharedObjects = vsg::SharedObjects::create();

    // TODO:
    // The SkyNode does this, but then it's awkward to add a SkyNode at runtime
    // because various other shaders depend on the define to activate lighting,
    // and those will have to be recompiled.
    // So instead we will just activate the lighting globally and rely on the 
    // light counts in the shader. Is this ok?
    instance.runtime().shaderCompileSettings->defines.insert("RK_LIGHTING");

    // read map from file:
    std::string infile; 
    if (commandLine.read({ "--map" }, infile))
    {
        auto map_file = rocky::util::readFromFile(infile);
        if (map_file.status.ok())
        {
            auto parse = mapNode->map->from_json(map_file.value);
            if (parse.ok())
            {
                if (mapNode->map->layers().empty())
                {
                    Log()->warn("No layers found in map file \"" + infile + "\"");
                }
            }
            else
            {
                Log()->warn("Failed to parse JSON from file \"" + infile + "\"");
            }
        }
        else
        {
            Log()->warn("Failed to read map from \"" + infile + "\"");
        }
    }

    // or read map from earth file:
    else if (commandLine.read({ "--earthfile" }, infile))
    {
        std::string msg;
        EarthFileImporter importer;
        auto result = importer.read(infile, instance.ioOptions());
        if (result.status.ok())
        {
            auto count = mapNode->map->layers().size();
            mapNode->map->from_json(result.value);

            if (count == mapNode->map->layers().size())
                msg = "Unable to import any layers from the earth file";

            Log()->info(json_pretty(result.value));
        }
        else
        {
            msg = "Failed to read earth file - " + result.status.message;
        }
        if (!msg.empty())
        {
            Log()->warn(msg);
        }
    }

    // install the ECS systems that will render components.
    ecs.systems.emplace_back(std::make_shared<MeshSystem>(entities));
    ecs.systems.emplace_back(std::make_shared<LineSystem>(entities));
    ecs.systems.emplace_back(std::make_shared<NodeSystem>(entities));
    ecs.systems.emplace_back(std::make_shared<IconSystem>(entities));
    ecs.systems.emplace_back(std::make_shared<LabelSystem>(entities));

    // install other ECS systems.
    ecs.systems.emplace_back(std::make_shared<EntityMotionSystem>(entities));

    // make a scene graph and connect all the renderer systems to it.
    // This way they will all receive the typical VSG traversals (accept, record, compile, etc.)
    ecs_node = ECS::VSG_SystemsGroup::create();
    ecs_node->connect(ecs);

    mainScene->addChild(ecs_node);
}

Application::~Application()
{
    entities.clear();
}

void
Application::onNextUpdate(std::function<void()> func)
{
    instance.runtime().runDuringUpdate(func);
}

void
Application::setupViewer(vsg::ref_ptr<vsg::Viewer> viewer)
{
    // Initialize the ECS subsystem:
    ecs_node->initialize(instance.runtime());

    // respond to the X or to hitting ESC
    // TODO: refactor this so it responds to individual windows and not the whole app?
    //viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // This sets up the internal tasks that will, for each command graph, record
    // a scene graph and submit the results to the renderer each frame. Also sets
    // up whatever's necessary to present the resulting swapchain to the device.
    vsg::CommandGraphs commandGraphs;
    for (auto iter : displayManager->_commandGraphByWindow)
    {
        commandGraphs.push_back(iter.second);
    }

    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);


#if 1
    // Configure a descriptor pool size that's appropriate for terrain
    // https://groups.google.com/g/vsg-users/c/JJQZ-RN7jC0/m/tyX8nT39BAAJ
    // https://www.reddit.com/r/vulkan/comments/8u9zqr/having_trouble_understanding_descriptor_pool/    
    // Since VSG dynamic allocates descriptor pools as it needs them,
    // this is not strictly necessary. But if you know something about the
    // types of descriptor sets you are going to use it will increase 
    // performance (?) to pre-allocate some pools like this.
    // There is a big trade-off since pre-allocating these pools takes up
    // a significant amount of memory.

    auto resourceHints = vsg::ResourceHints::create();

    // max number of descriptor sets per pool, regardless of type:
    resourceHints->numDescriptorSets = 1;

    // max number of descriptor sets of a specific type per pool:
    //resourceHints->descriptorPoolSizes.push_back(
    //    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });

    viewer->compile(resourceHints);

#else
    viewer->compile();
#endif
}

#if 0
void
Application::recreateViewer() 
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer, void());

    // Makes a new viewer, copying settings from the old viewer.
    vsg::EventHandlers handlers = viewer->getEventHandlers();

    // before we destroy it,
    // wait until the device is idle to avoid changing state while it's being used.
    viewer->deviceWaitIdle();

    viewer = createViewer();

    instance.runtime().viewer = viewer;
    
    for (auto i : displayConfiguration.windows)
        viewer->addWindow(i.first);

    for (auto& j : handlers)
        viewer->addEventHandler(j);

    setupViewer(viewer);
}
#endif

void
Application::realize()
{
    if (!_viewerRealized)
    {
        // Make a window if the user didn't.
        if (viewer->windows().empty() && autoCreateWindow)
        {
            displayManager->addWindow(vsg::WindowTraits::create(1920, 1080, "Main Window"));
        }

        setupViewer(viewer);

        // mark the viewer ready so that subsequent changes will know to
        // use an asynchronous path.
        _viewerRealized = true;
    }
}

int
Application::run()
{
    // The main frame loop
    while (frame() == true);
    return 0;
}

bool
Application::frame()
{
    ROCKY_PROFILE_FUNCTION();

    if (!viewer->compileManager)
    {
        realize();
    }

    auto t_start = std::chrono::steady_clock::now();

    if (!viewer->advanceToNextFrame())
        return false;

    auto t_update = std::chrono::steady_clock::now();

    // rocky map update pass - management of tiles and paged data
    mapNode->update(viewer->getFrameStamp());

    // ECS updates
    ecs.update(viewer->getFrameStamp()->time);
    ecs_node->update(instance.runtime());

    // User update
    if (updateFunction)
        updateFunction();

    auto num_windows = viewer->windows().size();

    // run through the viewer's update operations queue; this includes
    // update ops initialized by rocky (e.g. terrain tile merges) and
    // anything dispatched by calling runtime().runDuringUpdate().
    viewer->update();

    // integrate any compile results that may be pending
    instance.runtime().update();

    // if the number of windows has changed, skip to the next frame immediately
    if (num_windows != viewer->windows().size())
    {
        Log()->debug("Number of windows changed; skipping to next frame");
        return true;
    }

    auto t_events = std::chrono::steady_clock::now();

    // Event handling happens after updating the scene, otherwise
    // things like tethering to a moving node will be one frame behind
    viewer->handleEvents();

    if (!viewer->active())
        return false;

    auto t_record = std::chrono::steady_clock::now();

    viewer->recordAndSubmit();

    auto t_present = std::chrono::steady_clock::now();

    viewer->present();

    auto t_end = std::chrono::steady_clock::now();
    stats.frame = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start);
    stats.update = std::chrono::duration_cast<std::chrono::microseconds>(t_events - t_update);
    stats.events = std::chrono::duration_cast<std::chrono::microseconds>(t_record - t_events);
    stats.record = std::chrono::duration_cast<std::chrono::microseconds>(t_present - t_record);
    stats.present = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_present);

    return viewer->active();
}

std::string
Application::about() const
{
    std::stringstream buf;
    for (auto& a : instance.about())
        buf << a << std::endl;
    return buf.str();
}

void
Application::setViewer(vsg::ref_ptr<vsg::Viewer> in_viewer)
{
    viewer = in_viewer;
    instance.runtime().viewer = viewer;
    displayManager = std::make_shared<DisplayManager>(*this);
}
