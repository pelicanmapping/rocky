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

#include <vsg/app/CloseHandler.h>
#include <vsg/app/View.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/vk/State.h>
#include <vsg/io/read.h>
#include <vsg/text/Font.h>
#include <vsg/nodes/DepthSorted.h>

#include <vector>

using namespace ROCKY_NAMESPACE;

Application::Application(int& argc, char** argv) :
    instance()
{
    vsg::CommandLine commandLine(&argc, argv);

    commandLine.read(instance._impl->runtime.readerWriterOptions);
    _debuglayer = commandLine.read({ "--debug" });
    _apilayer = commandLine.read({ "--api" });
    _vsync = !commandLine.read({ "--novsync" });
    //_multithreaded = commandLine.read({ "--mt" });

    viewer = vsg::Viewer::create();

    root = vsg::Group::create();

    mainScene = vsg::Group::create();

    root->addChild(mainScene);

    mapNode = rocky::MapNode::create(instance);

    // the sun
    if (commandLine.read({ "--sky" }))
    {
        auto sky = rocky::SkyNode::create(instance);
        mainScene->addChild(sky);
    }

    mapNode->terrainSettings().concurrency = 4u;
    mapNode->terrainSettings().skirtRatio = 0.025f;
    mapNode->terrainSettings().minLevelOfDetail = 1;
    mapNode->terrainSettings().screenSpaceError = 135.0f;

    // wireframe overlay
    if (commandLine.read({ "--wire" }))
        instance.runtime().shaderCompileSettings->defines.insert("RK_WIREFRAME_OVERLAY");

    mainScene->addChild(mapNode);

    // Set up the runtime context with everything we need.
    // Eventually this should be automatic in InstanceVSG
    instance.runtime().compiler = [this]() { return viewer->compileManager; };
    instance.runtime().updates = [this]() { return viewer->updateOperations; };
    instance.runtime().sharedObjects = vsg::SharedObjects::create();
}

util::Future<vsg::ref_ptr<vsg::Window>>
Application::addWindow(vsg::ref_ptr<vsg::WindowTraits> traits)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(traits, { });

    util::Future<vsg::ref_ptr<vsg::Window>> future_window;

    auto add_window = [=]() mutable
    {
        auto result = future_window;

        //viewer->stopThreading();

        traits->debugLayer = _debuglayer;
        traits->apiDumpLayer = _apilayer;
        if (!_vsync)
            traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

        if (viewer->windows().size() > 0)
        {
            traits->shareWindow = viewer->windows().front();
        }

        auto window = vsg::Window::create(traits);

        // Each window gets its own CommandGraph. We will store it here and then
        // set it up later when the frame loop starts.
        auto commandgraph = vsg::CommandGraph::create(window);
        _commandGraphByWindow[window] = commandgraph;

        // main camera
        double nearFarRatio = 0.00001;
        double R = mapNode->mapSRS().ellipsoid().semiMajorAxis();
        double ar = (double)traits->width / (double)traits->height;

        auto camera = vsg::Camera::create(
            vsg::Perspective::create(30.0, ar, R * nearFarRatio, R * 20.0),
            vsg::LookAt::create(),
            vsg::ViewportState::create(0, 0, traits->width, traits->height));

        auto view = vsg::View::create(camera, mainScene);

        // add the new view to the window:
        if (_viewerRealized)
            addViewAfterViewerIsRealized(window, view);
        else
            addView(window, view);

        // Tell Rocky it needs to mutex-protect the terrain engine
        // now that we have more than one window.
        mapNode->terrainSettings().supportMultiThreadedRecord = true;

        // add the new window to our viewer
        viewer->addWindow(window);

        // install a manipulator for the new view:
        addManipulator(window, view);

        result.resolve(window);

        //if (_multithreaded)
        //{
        //    viewer->setupThreading();
        //}

        if (_viewerRealized)
        {
            _viewerDirty = true;
        }
    };

    if (_viewerRealized)
        instance.runtime().runDuringUpdate(add_window);
    else
        add_window();

    return future_window;
}

void
Application::addView(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view->camera != nullptr, void());

    if (_viewerRealized)
    {
        instance.runtime().runDuringUpdate([=]() {
            addViewAfterViewerIsRealized(window, view);
            });
    }

    else
    {
        // use this before realization:

        if (auto iter = _commandGraphByWindow.find(window); iter != _commandGraphByWindow.end())
        {
            auto commandgraph = iter->second;

            if (view->children.empty())
            {
                view->addChild(root);
            }

            auto rendergraph = vsg::RenderGraph::create(window, view);

            commandgraph->addChild(rendergraph);

            // remember so we can remove it later
            _renderGraphByView[view] = rendergraph;
            displayConfiguration.windows[window].emplace_back(view);
        }
    }
}

void
Application::addViewAfterViewerIsRealized(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    // Each view gets its own render pass:
    auto rendergraph = vsg::RenderGraph::create(window, view);

    if (view->children.empty())
    {
        view->addChild(root);
    }

    if (auto iter = _commandGraphByWindow.find(window); iter != _commandGraphByWindow.end())
    {
        auto commandgraph = iter->second;
        commandgraph->addChild(rendergraph);

        // Add this new view to the viewer's compile manager:
        viewer->compileManager->add(*window, view);

        // Compile the new render pass for this view.
        // The lambda idiom is taken from vsgexamples/dynamicviews
        auto result = viewer->compileManager->compile(rendergraph, [&view](vsg::Context& context)
            {
                return context.view == view.get();
            });

        if (result.requiresViewerUpdate())
        {
            vsg::updateViewer(*viewer, result);
        }
    }

    // remember so we can remove it later
    _renderGraphByView[view] = rendergraph;
    displayConfiguration.windows[window].emplace_back(view);

    // Add a manipulator - we might not do this by default - check back.
    addManipulator(window, view);
}

void
Application::removeView(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view != nullptr, void());

    auto remove = [=]()
    {
        auto ci = _commandGraphByWindow.find(window);
        ROCKY_SOFT_ASSERT_AND_RETURN(ci != _commandGraphByWindow.end(), void());
        auto& commandgraph = ci->second;

        auto ri = _renderGraphByView.find(view);
        ROCKY_SOFT_ASSERT_AND_RETURN(ri != _renderGraphByView.end(), void());
        auto& rendergraph = ri->second;

        // remove the render pass from the command graph.
        auto& rps = commandgraph->children;
        rps.erase(std::remove(rps.begin(), rps.end(), rendergraph), rps.end());

        _renderGraphByView.erase(view);
        auto& views = displayConfiguration.windows[vsg::observer_ptr<vsg::Window>(window)];
        views.erase(std::remove(views.begin(), views.end(), view), views.end());
    };

    if (_viewerRealized)
        instance.runtime().runDuringUpdate(remove);
    else
        remove();
}

void
Application::addPostRenderNode(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::Node> node)
{
    auto& commandGraph = _commandGraphByWindow[window];

    ROCKY_SOFT_ASSERT_AND_RETURN(commandGraph, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(commandGraph->children.size() > 0, void());

    commandGraph->addChild(node);
}

std::shared_ptr<Map>
Application::map()
{
    return mapNode->map();
}

void
Application::realizeViewer(vsg::ref_ptr<vsg::Viewer> viewer)
{
    // respond to the X or to hitting ESC
    // TODO: refactor this so it responds to individual windows and not the whole app?
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // This sets up the internal tasks that will, for each command graph, record
    // a scene graph and submit the results to the renderer each frame. Also sets
    // up whatever's necessary to present the resulting swapchain to the device.
    vsg::CommandGraphs commandGraphs;
    for (auto iter : _commandGraphByWindow)
    {
        commandGraphs.push_back(iter.second);
    }

    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

    // Configure a descriptor pool size that's appropriate for paged terrains
    // (they are a good candidate for DS reuse). This is optional.
    // https://groups.google.com/g/vsg-users/c/JJQZ-RN7jC0/m/tyX8nT39BAAJ
    auto resourceHints = vsg::ResourceHints::create();
    resourceHints->numDescriptorSets = 1024;
    resourceHints->descriptorPoolSizes.push_back(
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 });

    // Initialize and compile existing any Vulkan objects found in the scene
    // (passing in ResourceHints to guide the resources allocated).
    viewer->compile(resourceHints);
}

void
Application::recreateViewer()
{
    // Makes a new viewer, copying settings from the old viewer.

    vsg::EventHandlers handlers = viewer->getEventHandlers();

    viewer = vsg::Viewer::create();
    
    for (auto i : displayConfiguration.windows)
        viewer->addWindow(i.first);

    for (auto& j : handlers)
        viewer->addEventHandler(j);

    realizeViewer(viewer);
}

int
Application::run()
{
    // Make a window if the user didn't.
    if (viewer->windows().empty())
    {
        addWindow(vsg::WindowTraits::create(1920, 1080, "Main Window"));
    }

    realizeViewer(viewer);

    // mark the viewer ready so that subsequent changes will know to
    // use an asynchronous path.
    _viewerRealized = true;

    // The main frame loop
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();

        // since an event handler could deactivate the viewer:
        if (!viewer->active())
            break;

        // rocky update pass - management of tiles and paged data
        mapNode->update(viewer->getFrameStamp());

        // user's update function
        if (updateFunction)
        {
            updateFunction();
        }

        // run through the viewer's update operations queue; this includes update ops 
        // initialized by rocky (tile merges or MapObject adds)
        viewer->update();

        if (_viewerDirty)
        {
            _viewerDirty = false;
            recreateViewer();
            continue;
        }

        addAndRemoveObjects();

        viewer->recordAndSubmit();

        viewer->present();
    }

    return 0;
}

void
Application::addAndRemoveObjects()
{
    if (!_objectsToAdd.empty() || !_objectsToRemove.empty())
    {
        std::scoped_lock(_add_remove_mutex);

        std::list<util::Future<Addition>> in_progress;

        // Any new nodes in the scene? integrate them now
        for(auto& addition : _objectsToAdd)
        {
            if (addition.available() && addition->node)
            {
                // Add the node.
                // TODO: for now we're just lumping everying together here. 
                // Later we can decide to sort by pipeline, or use a spatial index, etc.
                mapNode->addChild(addition->node);

                // Update the viewer's tasks so they are aware of any new DYNAMIC data
                // elements present in the new nodes that they will need to transfer
                // to the GPU.
                if (!addition->compileResult)
                {
                    // If the node hasn't been compiled, do it now. This will usually happen
                    // if the node was created prior to the application loop starting up.
                    auto cr = viewer->compileManager->compile(addition->node);
                    if (cr.requiresViewerUpdate())
                    {
                        vsg::updateViewer(*viewer, cr);
                    }
                }
                else if (addition->compileResult.requiresViewerUpdate())
                {
                    vsg::updateViewer(*viewer, addition->compileResult);
                }
            }
            else
            {
                in_progress.push_back(addition);
            }
        }
        _objectsToAdd.swap(in_progress);

        // Remove anything in the remove queue
        while (!_objectsToRemove.empty())
        {
            auto& node = _objectsToRemove.front();
            if (node)
            {
                mapNode->children.erase(
                    std::remove(mapNode->children.begin(), mapNode->children.end(), node),
                    mapNode->children.end());
                    
            }
            _objectsToRemove.pop_front();
        }
    }
}

void
Application::add(shared_ptr<MapObject> obj)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(obj, void());

    // For each object attachment, create its node and then schedule it
    // for compilation and merging into the scene graph.
    for (auto& attachment : obj->attachments)
    {        
        // Tell the attachment to create a node if it doesn't already exist
        attachment->createNode(instance.runtime());

        if (attachment->node)
        {
            // calculate the bounds for a depthsorting node and possibly a cull group.
            vsg::ComputeBounds cb;
            attachment->node->accept(cb);
            auto bs = vsg::dsphere((cb.bounds.min + cb.bounds.max) * 0.5, vsg::length(cb.bounds.max - cb.bounds.min) * 0.5);

            // activate depth sorting.
            // the bin number must be >1 for sorting to activate. I am using 10 for no particular reason.
            auto node = vsg::DepthSorted::create();
            node->binNumber = 10;
            node->bound = bs;
            node->child = attachment->node;

            if (attachment->underGeoTransform)
            {
                if (attachment->horizonCulling)
                {
                    if (!obj->horizoncull)
                    {
                        obj->horizoncull = HorizonCullGroup::create();
                        obj->horizoncull->bound = bs;
                        obj->xform->addChild(obj->horizoncull);
                    }
                    obj->horizoncull->addChild(node);
                }
                else
                {
                    auto cullGroup = vsg::CullGroup::create(bs);
                    cullGroup->addChild(node);
                    obj->xform->addChild(cullGroup);
                }
            }
            else
            {
                auto cullGroup = vsg::CullGroup::create(bs);
                cullGroup->addChild(node);
                obj->root->addChild(cullGroup);
            }
        }
    }

    auto my_viewer = viewer;
    auto my_node = obj->root;

    auto compileNode = [my_viewer, my_node](Cancelable& c)
    {
        vsg::CompileResult cr;
        if (my_viewer->compileManager && !c.canceled())
        {
            cr = my_viewer->compileManager->compile(my_node);
        }
        return Addition{ my_node, cr };
    };

    // TODO: do we want a specific job pool for compiles, or
    // perhaps a single thread that compiles things from a queue?
    {
        std::scoped_lock L(_add_remove_mutex);
        _objectsToAdd.emplace_back(util::job::dispatch(compileNode));
    }
}

void
Application::remove(shared_ptr<MapObject> obj)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(obj, void());

    std::scoped_lock(_add_remove_mutex);
    _objectsToRemove.push_back(obj->root);
}

void
Application::addManipulator(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    auto manip = rocky::MapManipulator::create(mapNode, window, view->camera);
    view->setObject("manip", manip);

    auto& ehs = viewer->getEventHandlers();

    // remove all the MapManipulators using the dumb remove-erase idiom
    ehs.erase(
        std::remove_if(
            ehs.begin(), ehs.end(),
            [](const vsg::ref_ptr<vsg::Visitor>& v) { return dynamic_cast<MapManipulator*>(v.get()); }),
        ehs.end()
    );

    // re-add them in the right order:
    for (auto& window : displayConfiguration.windows)
    {
        for(auto vi = window.second.rbegin(); vi != window.second.rend(); ++vi)
        {
            auto& view = *vi;
            auto manip = view->getRefObject<MapManipulator>("manip");
            ehs.push_back(manip);
        }
    }
}
