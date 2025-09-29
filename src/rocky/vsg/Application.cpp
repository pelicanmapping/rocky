/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
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
#include "ecs/NodeGraph.h"

#include <rocky/contrib/EarthFileImporter.h>

#ifdef ROCKY_HAS_IMGUI
#include <rocky/rocky_imgui.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    Result<> loadMapFile(const std::string& location, MapNode& mapNode, Context context)
    {
        auto map_file = URI(location).read(context->io);
        if (map_file.failed())
            return map_file.error();

        auto r = mapNode.from_json(map_file->content.data, context->io.from(location));
        if (r.failed())
            return r.error();

        return ResultVoidOK;
    }

    Result<> importEarthFile(const std::string& infile, MapNode& mapNode, Context context)
    {
        auto io = context->io.from(infile);

        EarthFileImporter importer;
        auto result = importer.read(infile, io);
        if (result.failed())
            return result.error();

        auto count = mapNode.map->layers().size();

        auto r = mapNode.from_json(result.value(), io);
        if (r.failed())
            return r.error();

        if (count == mapNode.map->layers().size())
            return Failure(Failure::ResourceUnavailable, "No layers imported from earth file");

        return ResultVoidOK;
    }
}

Application::Application()
{
    int argc = 0;
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

Application::Application(vsg::ref_ptr<vsg::Viewer> viewer_)
    : viewer(viewer_)
{
    int argc = 0;
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

Application::Application(int& argc, char** argv)
{
    ctor(argc, argv);
}

Application::Application(vsg::ref_ptr<vsg::Viewer> viewer_, int& argc, char** argv) :
    viewer(viewer_)
{
    ctor(argc, argv);
}

void
Application::ctor(int& argc, char** argv)
{
    if (!viewer)
    {
        viewer = vsg::Viewer::create();
    }

    // new VSG context
    vsgcontext = VSGContextFactory::create(viewer, argc, argv);

    if (!vsgcontext->status.ok())
    {
        Log()->error("Cannot create rocky context: {}" + vsgcontext->status.error().message);
        return;
    }

    // new display manager
    display.initialize(*this);

    // parse the command line
    vsg::CommandLine commandLine(&argc, argv);
    commandLine.read(vsgcontext->readerWriterOptions);
    _debuglayer = commandLine.read("--debug");
    _apilayer = commandLine.read("--api");
    _vsync = !commandLine.read({ "--novsync", "--no-vsync" });

    if (commandLine.read("--pause")) ::getchar();

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

    mapNode = rocky::MapNode::create(vsgcontext);

    // the sun
    if (commandLine.read("--sky"))
    {
        skyNode = rocky::SkyNode::create(vsgcontext);
        mainScene->addChild(skyNode);
    }

    // set on-demand rendering mode from the command line
    if (commandLine.read("--on-demand"))
    {
        renderContinuously = false;
    }
    else if (commandLine.read("--continuous"))
    {
        renderContinuously = true;
    }

    // a node to render the map/terrain
    mainScene->addChild(mapNode);

    // No idea what this actually does :)
    if (commandLine.read("--mt"))
    {
        viewer->setupThreading();
    }

    vsgcontext->sharedObjects = vsg::SharedObjects::create();

    // read map from file:
    std::string infile; 
    if (commandLine.read("--map", infile))
    {
        auto r = loadMapFile(infile, *mapNode, vsgcontext);
        if (r.failed())
            commandLineStatus = r.error();
    }

    // import map from an osgEarth earth file:
    if (commandLine.read({ "--earthfile", "--earth-file" }, infile) && commandLineStatus.ok())
    {
        auto r = importEarthFile(infile, *mapNode, vsgcontext);
        if (r.failed())
            commandLineStatus = r.error();
    }

    bool indirect = false;
    if (commandLine.read({ "--indirect" }))
    {
        indirect = true;
    }

    // if there are any command-line arguments remaining, assume the first is a map file.
    if (commandLine.argc() > 1 && commandLineStatus.ok())
    {
        auto r = loadMapFile(commandLine[1], *mapNode, vsgcontext);
        if (r.failed())
            commandLineStatus = r.error();
    }

    // Create the ECS system manager and all its default systems.
    ecsNode = ECSNode::create(registry, false);

    // Responds to changes in Transform components by updating the scene graph
    auto xform_system = TransformSystem::create(registry);
    _subs += xform_system->onChanges([&]() { vsgcontext->requestFrame(); });
    ecsNode->add(xform_system);

    // Rendering components:
    ecsNode->add(MeshSystemNode::create(registry));
    ecsNode->add(NodeSystemNode::create(registry));
    ecsNode->add(LineSystemNode::create(registry));

    if (indirect)
        ecsNode->add(IconSystem2Node::create(registry));
    else
        ecsNode->add(IconSystemNode::create(registry));
    
    ecsNode->add(LabelSystemNode::create(registry));

#ifdef ROCKY_HAS_IMGUI
    ecsNode->add(WidgetSystemNode::create(registry));
#endif

    mainScene->addChild(ecsNode);
}

Application::~Application()
{
    Log()->info("Quitting background services...");
    background.quit();
}

void
Application::onNextUpdate(std::function<void()> func)
{
    vsgcontext->onNextUpdate(func);
}

void
Application::setupViewer(vsg::ref_ptr<vsg::Viewer> viewer)
{
    // share the same queue family as the graphics command graph for now.
    auto computeCommandGraph = vsgcontext->getOrCreateComputeCommandGraph(
        display.sharedDevice(),
        display._commandGraphByWindow.begin()->second->queueFamily);

    // Initialize the ECS subsystem:
    if (ecsNode)
    {
        ecsNode->initialize(vsgcontext);
    }

    // respond to the X or to hitting ESC
    // TODO: refactor this so it responds to individual windows and not the whole app?
    //viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // This sets up the internal tasks that will, for each command graph, record
    // a scene graph and submit the results to the renderer each frame. Also sets
    // up whatever's necessary to present the resulting swapchain to the device.
    vsg::CommandGraphs commandGraphs{ computeCommandGraph };

    for (auto iter : display._commandGraphByWindow)
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

    //#if VSG_API_VERSION_GREATER_EQUAL(1,1,8)
    //    resourceHints->numDatabasePagerReadThreads = 8;
    //#endif

    // max number of descriptor sets of a specific type per pool:
    //resourceHints->descriptorPoolSizes.push_back(
    //    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });

    viewer->compile(resourceHints);

    // Force VSG to install a DatabasePager.
    //vsg::CompileResult result;
    //result.containsPagedLOD = true;
    //vsg::updateTasks(viewer->recordAndSubmitTasks, viewer->compileManager, result);

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
            if (app.ecsNode)
            {
                app.ecsNode->update(app.vsgcontext);
            }

            // keep the frames running if the pager is active
            auto& tasks = app.viewer->recordAndSubmitTasks;
            if (!tasks.empty() && tasks[0]->databasePager && tasks[0]->databasePager->numActiveRequests > 0)
            {
                app.vsgcontext->requestFrame();
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
            display.addWindow(traits);
        }

        setupViewer(viewer);

        // install our frame update operation
        viewer->updateOperations->add(AppUpdateOperation::create(*this), vsg::UpdateOperations::ALL_FRAMES);

        // mark the viewer ready so that subsequent changes will know to
        // use an asynchronous path.
        _viewerRealized = true;
    }
}

std::uint64_t
Application::frameCount() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer, 0);
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer->getFrameStamp(), 0);
    return viewer->getFrameStamp()->frameCount;
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
    if (vsgcontext->renderRequests.exchange(0) > 0)
        _framesUntilStopRender = 2;

    vsgcontext->renderingEnabled =
        renderContinuously == true ||
        _framesUntilStopRender-- > 0;

    if (vsgcontext->renderingEnabled)
    {
        if (!viewer->advanceToNextFrame())
        {
            _lastFrameOK = false;
            return false;
        }

        t_events = std::chrono::steady_clock::now();

        viewer->handleEvents();

        if (!viewer->active())
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

        t_record = std::chrono::steady_clock::now();

        viewer->recordAndSubmit();

        t_present = std::chrono::steady_clock::now();

        viewer->present();

        auto t_end = std::chrono::steady_clock::now();
        stats.frame = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start);
        stats.events = std::chrono::duration_cast<std::chrono::microseconds>(t_update - t_events);
        stats.update = std::chrono::duration_cast<std::chrono::microseconds>(t_record - t_update);
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

        viewer->handleEvents();

        // update traversal (see AppUpateOperation)
        viewer->update();

        // it's possible that an update operation will shut down the viewer:
        if (!viewer->active())
        {
            return false;
        }

        // Call the user-supplied "idle" functions
        for (auto& idle : idleFunctions)
        {
            if (idle) (*idle)();
        }

        _framesSinceLastRender++;

        // After not rendering for a few frames, start applying a sleep to
        // "simulate" vsync so we don't tax the CPU by running full-out.
        if (_framesSinceLastRender >= 60 && vsgcontext->renderRequests == 0)
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
    for (auto& a : vsgcontext->about())
        buf << a << std::endl;
    return buf.str();
}


#ifndef ROCKY_HAS_IMGUI
using RenderImGuiContext = vsg::Node;
#endif

void
Application::install(vsg::ref_ptr<RenderImGuiContext> group)
{
    install(group, true);
}

void
Application::install(vsg::ref_ptr<RenderImGuiContext> group, bool installAutomaticIdleFunctions)
{
#ifdef ROCKY_HAS_IMGUI
    ROCKY_SOFT_ASSERT_AND_RETURN(group, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(group->window, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(group->imguiContext(), void());

    auto view = group->view ? group->view : display.windowsAndViews[group->window].front();
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // keep track so we can remove it later if necessary:
    auto& viewData = display._viewData[view];
    viewData.guiContextGroup = group;

    // add the renderer to the view's render graph:
    viewData.parentRenderGraph->addChild(group);

    // add the event handler that will pass events from VSG to ImGui:
    auto send = SendEventsToImGuiContext::create(group->window, group->imguiContext());
    viewData.guiEventVisitor = send;
    auto& handlers = display.vsgcontext->viewer()->getEventHandlers();
    handlers.insert(handlers.begin(), send);

    // request a frame when the sender handles an ImGui event:
    _subs += send->onEvent([vsgcontext(this->vsgcontext)](const vsg::UIEvent& e)
        {
            if (e.cast<vsg::FrameEvent>()) return;
            vsgcontext->requestFrame();
        });

    if (installAutomaticIdleFunctions)
    {
        // when the user adds a new GUI node, we need to add it to the idle functions
        _subs += group->onNodeAdded([app(this), ic(group->imguiContext())](vsg::ref_ptr<ImGuiContextNode> node)
            {
                // add the node to the idle functions so it can render
                auto idle_function = [ic, node]()
                    {
                        ImGui::SetCurrentContext(ic);
                        ImGui::NewFrame();
                        node->render(ic);
                        ImGui::EndFrame();
                    };

                app->idleFunctions.emplace_back(std::make_shared<std::function<void()>>(idle_function));
            });
    }
#endif
}

