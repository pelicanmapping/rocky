/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Application.h"

#include "ecs/MeshSystem.h"
#include "ecs/LineSystem.h"
#include "ecs/IconSystem.h"
#include "ecs/IconSystem2.h"
#include "ecs/LabelSystem.h"
#include "ecs/WidgetSystem.h"
#include "ecs/TransformSystem.h"

#include <rocky/contrib/EarthFileImporter.h>

#include <vsg/app/CloseHandler.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/core/Version.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    Status loadMapFile(const std::string& location, MapNode& mapNode, Context context)
    {
        Status status;

        auto map_file = URI(location).read(context->io);

        if (map_file.status.ok())
        {
            auto parse_result = mapNode.from_json(map_file->data, context->io.from(location));
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

    Status importEarthFile(const std::string& infile, MapNode& mapNode, Context context)
    {
        Status status;

        auto io = context->io.from(infile);

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
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

Application::Application(vsg::ref_ptr<vsg::Viewer> viewer_)
{
    int argc = 0;
    const char* argv[1] = { "rocky" };
    setViewer(viewer_);
    ctor(argc, (char**)argv);
}

Application::Application(int& argc, char** argv)
{
    ctor(argc, argv);
}

Application::Application(vsg::ref_ptr<vsg::Viewer> viewer_, int& argc, char** argv)
{
    setViewer(viewer_);
    ctor(argc, argv);
}

void
Application::ctor(int& argc, char** argv)
{
    if (!viewer)
    {
        setViewer(vsg::Viewer::create());
    }

    context = VSGContextFactory::create(viewer, argc, argv);

    displayManager->initialize(context);

    vsg::CommandLine commandLine(&argc, argv);

    commandLine.read(context->readerWriterOptions);
    _debuglayer = commandLine.read("--debug");
    _apilayer = commandLine.read("--api");
    _vsync = !commandLine.read({ "--novsync", "--no-vsync" });

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
            << "    [--map <filename>]        // load a JSON map file" << std::endl
            << "    [--earth-file <filename>] // import an osgEarth earth file" << std::endl
            << "    [--no-vsync]              // disable vertical sync" << std::endl
            << "    [--continuous]            // render frames continuously (instead of only when needed)" << std::endl
            << "    [--log-level <level>]     // set the log level (debug, info, warn, error, critical, off)" << std::endl
            << "    [--sky]                   // install a rudimentary lighting model" << std::endl
            << "    [--version]               // print the version" << std::endl
            << "    [--version-all]           // print all dependency versions" << std::endl
            << "    [--debug]                 // activate the Vulkan debug validation layer" << std::endl
            << "    [--api]                   // activate the Vulkan API validation layer (mega-verbose)" << std::endl
            ;

        exit(0);
    }

    root = vsg::Group::create();
    mainScene = vsg::Group::create();
    root->addChild(mainScene);

    mapNode = rocky::MapNode::create(context);

    // the sun
    if (commandLine.read("--sky"))
    {
        skyNode = rocky::SkyNode::create(context);
        mainScene->addChild(skyNode);
    }

    // wireframe overlay
    if (commandLine.read("--wire"))
    {
        context->shaderCompileSettings->defines.insert("ROCKY_WIREFRAME_OVERLAY");
    }

    // a node to render the map/terrain
    mainScene->addChild(mapNode);

    // No idea what this actually does :)
    if (commandLine.read("--mt"))
    {
        viewer->setupThreading();
    }

    context->sharedObjects = vsg::SharedObjects::create();

    // TODO:
    // The SkyNode does this, but then it's awkward to add a SkyNode at runtime
    // because various other shaders depend on the define to activate lighting,
    // and those will have to be recompiled.
    // So instead we will just activate the lighting globally and rely on the 
    // light counts in the shader. Is this ok?
    context->shaderCompileSettings->defines.insert("ROCKY_LIGHTING");

    // read map from file:
    std::string infile; 
    if (commandLine.read("--map", infile))
    {
        commandLineStatus = loadMapFile(infile, *mapNode, context);
    }

    // import map from an osgEarth earth file:
    if (commandLine.read({ "--earthfile", "--earth-file" }, infile) && commandLineStatus.ok())
    {
        commandLineStatus = importEarthFile(infile, *mapNode, context);
    }

    bool indirect = false;
    if (commandLine.read({ "--indirect" }))
    {
        indirect = true;
    }

    // if there are any command-line arguments remaining, assume the first is a map file.
    if (commandLine.argc() > 1 && commandLineStatus.ok())
    {
        commandLineStatus = loadMapFile(commandLine[1], *mapNode, context);
    }


    // Create the ECS system manager and all its default systems.
    ecsManager = ecs::ECSNode::create(registry);

    // Responds to changes in Transform components by updating the scene graph
    auto xform_system = TransformSystem::create(registry);
    xform_system->onChanges([&]() { context->requestFrame(); });
    ecsManager->add(xform_system);

    // Rendering components:
    ecsManager->add(MeshSystemNode::create(registry));
    ecsManager->add(NodeSystemNode::create(registry));
    ecsManager->add(LineSystemNode::create(registry));

    if (indirect)
        ecsManager->add(IconSystem2Node::create(registry));
    else
        ecsManager->add(IconSystemNode::create(registry));
    
    ecsManager->add(LabelSystemNode::create(registry));

#ifdef ROCKY_HAS_IMGUI
    ecsManager->add(WidgetSystemNode::create(registry));
#endif

    mainScene->addChild(ecsManager);
}

Application::~Application()
{
    Log()->info("Quitting background services...");
    backgroundServices.quit();
}

void
Application::onNextUpdate(std::function<void()> func)
{
    context->onNextUpdate(func);
}

void
Application::setupViewer(vsg::ref_ptr<vsg::Viewer> viewer)
{
    // share the same queue family as the graphics command graph for now.
    auto computeCommandGraph = context->getOrCreateComputeCommandGraph(
        displayManager->sharedDevice(),
        displayManager->_commandGraphByWindow.begin()->second->queueFamily);

    // Initialize the ECS subsystem:
    ecsManager->initialize(context);

    // respond to the X or to hitting ESC
    // TODO: refactor this so it responds to individual windows and not the whole app?
    //viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // This sets up the internal tasks that will, for each command graph, record
    // a scene graph and submit the results to the renderer each frame. Also sets
    // up whatever's necessary to present the resulting swapchain to the device.
    vsg::CommandGraphs commandGraphs{ computeCommandGraph };

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
    //resourceHints->numDescriptorSets = 1;

#if VSG_API_VERSION_GREATER_EQUAL(1,1,8)
    resourceHints->numDatabasePagerReadThreads = 8;
#endif

    // max number of descriptor sets of a specific type per pool:
    //resourceHints->descriptorPoolSizes.push_back(
    //    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });

    viewer->compile(resourceHints);

    // Force VSG to install a DatabasePager.
    vsg::CompileResult result;
    result.containsPagedLOD = true;
    vsg::updateTasks(viewer->recordAndSubmitTasks, viewer->compileManager, result);

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

    instance->viewer = viewer;
    
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
            // ECS updates - rendering or modifying entities
            app.ecsManager->update(app.context);

            // Context update callbacks
            app.context->onUpdate.fire();

            // keep the frames running if the pager is active
            auto& tasks = app.viewer->recordAndSubmitTasks;
            if (!tasks.empty() && tasks[0]->databasePager && tasks[0]->databasePager->numActiveRequests > 0)
            {
                app.context->requestFrame();
            }
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
            auto traits = vsg::WindowTraits::create(1920, 1080, "Main Window");
            traits->queueFlags |= VK_QUEUE_COMPUTE_BIT;
            traits->synchronizationLayer = true;
            displayManager->addWindow(traits);
        }

        setupViewer(viewer);

        // install our frame update operation
        viewer->updateOperations->add(AppUpdateOperation::create(*this), vsg::UpdateOperations::ALL_FRAMES);

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
    //Log()->info("Frame start ----------------------------------------------------------------------------");

    //viewer->deviceWaitIdle();
    //viewer->waitForFences(1, ~0);

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
    context->renderingEnabled =
        context->renderContinuously == true ||
        context->renderRequests.exchange(0) > 0;

    if (context->renderingEnabled)
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
        viewer->pollEvents(_framesSinceLastRender > 0);
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
        for (auto& f : noRenderFunctions)
        {
            if (f) (*f)();
        }

        _framesSinceLastRender++;

        // After not rendering for a few frames, start applying a sleep to
        // "simulate" vsync so we don't tax the CPU by running full-out.
        if (_framesSinceLastRender >= 60 && context->renderRequests == 0)
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
    }

    return viewer->active();
}


std::string
Application::about() const
{
    std::stringstream buf;
    for (auto& a : context->about())
        buf << a << std::endl;
    return buf.str();
}

void
Application::setViewer(vsg::ref_ptr<vsg::Viewer> in_viewer)
{
    viewer = in_viewer;
    displayManager = std::make_shared<DisplayManager>(*this);
}
