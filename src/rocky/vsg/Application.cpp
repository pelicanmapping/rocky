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
#include <vsg/vk/CommandBuffer.h>
#include <vsg/io/read.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    Status loadMapFile(const std::string& location, MapNode& mapNode, Instance& instance)
    {
        Status status;

        auto map_file = URI(location).read(instance.io());

        if (map_file.status.ok())
        {
            auto parse_result = mapNode.from_json(map_file->data, instance.io().from(location));
            if (parse_result.failed())
            {
                status = parse_result;
            }
        }
        else
        {
            // return the error
            status = map_file.status;
        }

        return status;
    }

    Status importEarthFile(const std::string& infile, MapNode& mapNode, Instance& instance)
    {
        Status status;

        auto io = instance.io().from(infile);

        EarthFileImporter importer;
        auto result = importer.read(infile, io);

        if (result.status.ok())
        {
            auto count = mapNode.map->layers().size();

            mapNode.from_json(result.value, io);

            if (count == mapNode.map->layers().size())
            {
                status = Status(Status::ResourceUnavailable, "No layers imported from earth file");
            }
        }
        else
        {
            status = result.status;
        }

        return status;
    }
}

Application::Application()
{
    int argc = 0;
    char* argv[1] = { "rocky" };
    ctor(argc, argv);
}

Application::Application(int& argc, char** argv) :
    instance(argc, argv)
{
    ctor(argc, argv);
}

void
Application::ctor(int& argc, char** argv)
{
    vsg::CommandLine commandLine(&argc, argv);

    commandLine.read(instance.runtime().readerWriterOptions);
    _debuglayer = commandLine.read("--debug");
    _apilayer = commandLine.read("--api");
    _vsync = !commandLine.read("--novsync");

    if (commandLine.read("--version"))
    {
        std::cout << "rocky " << ROCKY_VERSION_STRING << std::endl;
        exit(0);
    }

    if (commandLine.read("--version-all"))
    {
        std::cout << about() << std::endl;
        exit(0);
    }

    if (commandLine.read("--help"))
    {
        std::cout
            << "rocky " << ROCKY_VERSION_STRING << std::endl
            << argv[0] << std::endl
            << "    [--map <filename>]       // load a JSON map file" << std::endl
            << "    [--earthfile <filename>] // import an osgEarth earth file" << std::endl
            << "    [--sky]                  // install a sky lighting model" << std::endl
            << "    [--version]              // print the version" << std::endl
            << "    [--verison-all]          // print all dependency versions" << std::endl
            << "    [--debug]                // activate the Vulkan debug validation layer" << std::endl
            << "    [--api]                  // activate the Vulkan API validation layer (mega-verbose)" << std::endl
            ;

        exit(0);
    }

    root = vsg::Group::create();

    mainScene = vsg::Group::create();
    root->addChild(mainScene);

    mapNode = rocky::MapNode::create(instance);

    // the sun
    if (commandLine.read("--sky"))
    {
        skyNode = rocky::SkyNode::create(instance);
        mainScene->addChild(skyNode);
    }

    // wireframe overlay
    if (commandLine.read("--wire"))
    {
        instance.runtime().shaderCompileSettings->defines.insert("ROCKY_WIREFRAME_OVERLAY");
    }

    // a node to render the map/terrain
    mainScene->addChild(mapNode);

    // Set up the runtime context with everything we need.
    setViewer(vsg::Viewer::create());

    // No idea what this actually does :)
    if (commandLine.read("--mt"))
    {
        viewer->setupThreading();
    }

    instance.runtime().sharedObjects = vsg::SharedObjects::create();

    // TODO:
    // The SkyNode does this, but then it's awkward to add a SkyNode at runtime
    // because various other shaders depend on the define to activate lighting,
    // and those will have to be recompiled.
    // So instead we will just activate the lighting globally and rely on the 
    // light counts in the shader. Is this ok?
    instance.runtime().shaderCompileSettings->defines.insert("ROCKY_LIGHTING");

    // read map from file:
    std::string infile; 
    if (commandLine.read("--map", infile))
    {
        commandLineStatus = loadMapFile(infile, *mapNode, instance);
    }

    // import map from an osgEarth earth file:
    if (commandLine.read("--earthfile", infile) && commandLineStatus.ok())
    {
        commandLineStatus = importEarthFile(infile, *mapNode, instance);
    }

    // if there are any command-line arguments remaining, assume the first is a map file.
    if (commandLine.argc() > 1 && commandLineStatus.ok())
    {
        commandLineStatus = loadMapFile(commandLine[1], *mapNode, instance);
    }

    ecsManager = ecs::SystemsManagerGroup::create(backgroundServices);

    ecsManager->add<MeshSystemNode>(entities);
    ecsManager->add<LineSystemNode>(entities);
    ecsManager->add<NodeSystemNode>(entities);
    ecsManager->add<IconSystemNode>(entities);
    ecsManager->add<LabelSystemNode>(entities);

    mainScene->addChild(ecsManager);
}

Application::~Application()
{
    Log()->info("Quitting background services...");
    backgroundServices.quit();
    handle.join();
}

void
Application::onNextUpdate(std::function<void()> func)
{
    instance.runtime().onNextUpdate(func);
}

void
Application::setupViewer(vsg::ref_ptr<vsg::Viewer> viewer)
{
    // Initialize the ECS subsystem:
    ecsManager->initialize(instance.runtime());

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

namespace
{
    /**
    * Framely operation that runs all of our update logic.
    */
    class AppUpdateOperation : public vsg::Inherit<vsg::Operation, AppUpdateOperation>
    {
    public:
        Application& app;

        AppUpdateOperation(Application& in_app) : app(in_app) {}

        void run() override
        {
            // MapNode updates - paging tiles in an out
            app.mapNode->update(app.viewer->getFrameStamp());
            
            // ECS updates - rendering or modifying entities
            app.ecsManager->update(app.instance.runtime());

            // User update
            if (app.updateFunction)
            {
                app.updateFunction();
            }

            // integrate any pending compile results or disposals
            app.instance.runtime().update();
        }
    };
}

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

        // install our frame update operation
        viewer->updateOperations->add(AppUpdateOperation::create(*this), vsg::UpdateOperations::ALL_FRAMES);

        // mark the viewer ready so that subsequent changes will know to
        // use an asynchronous path.
        _viewerRealized = true;

        if (instance.renderOnDemand())
        {
            Log()->debug("Render-on-demand mode enabled");
        }
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
    _lastFrameOK = true;

    // for stats collection
    std::chrono::steady_clock::time_point t_start, t_update, t_events, t_record, t_present, t_end;

    // realize on first frame if not already realized
    if (!viewer->compileManager)
    {
        realize();
    }

    t_start = std::chrono::steady_clock::now();

    // whether we need to render a new frame based on the renderOnDemand state:
    runtime().renderingEnabled =
        instance.renderOnDemand() == false ||
        instance.runtime().renderRequests.exchange(0) > 0;

    if (runtime().renderingEnabled)
    {
        if (!viewer->advanceToNextFrame())
        {
            _lastFrameOK = false;
            return false;
        }

        t_update = std::chrono::steady_clock::now();

        auto num_windows = viewer->windows().size();

        // Update the scene graph (see AppUpdateOperation)
        viewer->update();

        // it's possible that an update operation will shut down the viewer:
        if (!viewer->active())
        {
            _lastFrameOK = false;
            return false;
        }

        // if the number of windows has changed, skip to the next frame immediately
        if (num_windows != viewer->windows().size())
        {
            Log()->debug("Number of windows changed; skipping to next frame");
            return true;
        }

        t_events = std::chrono::steady_clock::now();

        // Event handling happens after updating the scene, otherwise
        // things like tethering to a moving node will be one frame behind
        viewer->handleEvents();

        if (!viewer->active())
        {
            _lastFrameOK = false;
            return false;
        }

        t_record = std::chrono::steady_clock::now();

        viewer->recordAndSubmit();

        t_present = std::chrono::steady_clock::now();

        viewer->present();

        auto t_end = std::chrono::steady_clock::now();
        stats.frame = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start);
        stats.update = std::chrono::duration_cast<std::chrono::microseconds>(t_events - t_update);
        stats.events = std::chrono::duration_cast<std::chrono::microseconds>(t_record - t_events);
        stats.record = std::chrono::duration_cast<std::chrono::microseconds>(t_present - t_record);
        stats.present = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_present);

        _framesSinceLastRender = 0;
    }

    else // no render
    {
        // manually poll the events and install a frame event
        // (normally called by advanceToNextFrame)
        viewer->pollEvents(false);
        viewer->getEvents().emplace_back(new vsg::FrameEvent(vsg::ref_ptr<vsg::FrameStamp>(viewer->getFrameStamp())));

        // update traversal (see AppUpateOperation)
        viewer->update();

        // it's possible that an update operation will shut down the viewer:
        if (!viewer->active())
        {
            return false;
        }

        // Event handling happens after updating the scene, otherwise
        // things like tethering to a moving node will be one frame behind
        viewer->handleEvents();

        // Call the user-supplied "no render" function
        if (noRenderFunction)
        {
            noRenderFunction();
        }

        _framesSinceLastRender++;

        // After not rendering for a few frames, start applying a sleep to
        // "simulate" vsync so we don't tax the CPU by running full-out.
        if (_framesSinceLastRender >= 60)
        {
            auto max_frame_time = std::chrono::milliseconds(10);
            auto now = vsg::clock::now();
            auto elapsed = now - t_start;
            if (elapsed < max_frame_time)
            {
                auto dur_us = std::chrono::duration_cast<std::chrono::microseconds>(max_frame_time - elapsed);
                std::this_thread::sleep_for(dur_us);
            }
        }

        auto t_end = std::chrono::steady_clock::now();

        stats.record = std::chrono::microseconds(0);
        stats.present = std::chrono::microseconds(0);
        stats.frame = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start);
    }

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

    //instance.runtime().offlineCompileManager = vsg::CompileManager::create(*viewer, vsg::ref_ptr<vsg::ResourceHints>{});

    displayManager = std::make_shared<DisplayManager>(*this);
}
