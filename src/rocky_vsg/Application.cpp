/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Application.h"
#include "MapNode.h"
#include "TerrainNode.h"
#include "MapManipulator.h"
#include "SkyNode.h"
#include "json.h"
#include <rocky/Ephemeris.h>
#include <rocky/Horizon.h>

#include <vsg/app/CloseHandler.h>
#include <vsg/app/View.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/nodes/Light.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/vk/State.h>

#include <vsg/text/StandardLayout.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/Font.h>
#include <vsg/io/read.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::engine;


Application::Application(int& argc, char** argv) :
    instance()
{
    vsg::CommandLine commandLine(&argc, argv);

    commandLine.read(instance._impl->runtime.readerWriterOptions);
    _debuglayer = commandLine.read({ "--debug" });
    _apilayer = commandLine.read({ "--api" });
    _vsync = !commandLine.read({ "--novsync" });

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

    mapNode->terrainNode()->concurrency = 4u;
    mapNode->terrainNode()->skirtRatio = 0.025f; 
    mapNode->terrainNode()->minLevelOfDetail = 1;
    mapNode->terrainNode()->screenSpaceError = 135.0f;

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

void
Application::createMainWindow(int width, int height, const std::string& name)
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
Application::map()
{
    return mapNode->map();
}

int
Application::run()
{
    // Make a window if the user didn't.
    if (!mainWindow)
    {
        createMainWindow(1920, 1080);
    }

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

    // respond to the X or to hitting ESC
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // default map manipulator
    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, camera));

    // View pairs a camera with a scene graph and manages
    // view-dependent state like lights and viewport.
    auto view = vsg::View::create(camera);
    view->addChild(root);
    
    // RenderGraph encapsulates vkCmdRenderPass/vkCmdEndRenderPass and owns things
    // like the clear color, render area, and a render target (framebuffer or window).
    auto renderGraph = vsg::RenderGraph::create(mainWindow, view);

    // CommandGraph holds the command buffers that the vk record/submit task
    // will use during record traversal.
    auto commandGraph = vsg::CommandGraph::create(mainWindow);
    commandGraph->addChild(renderGraph);

    // This sets up the internal tasks that will, for each command graph, record
    // a scene graph and submit the results to the renderer each frame. Also sets
    // up whatever's necessary to present the resulting swapchain to the device.
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

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

    // Use a separate thread for each CommandGraph?
    // https://groups.google.com/g/vsg-users/c/-YRI0AxPGDQ/m/A2EDd5T0BgAJ
    viewer->setupThreading();

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

        // handle any object additions.
        processAdditionsAndRemovals();

        viewer->recordAndSubmit();

        viewer->present();
    }

    return 0;
}

void
Application::processAdditionsAndRemovals()
{
    if (!_additions.empty() || !_removals.empty())
    {
        std::scoped_lock(_add_remove_mutex);

        std::list<util::Future<Addition>> in_progress;

        // Any new nodes in the scene? integrate them now
        for(auto& addition : _additions)
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
        _additions.swap(in_progress);

        // Remove anything in the remove queue
        while (!_removals.empty())
        {
            auto& node = _removals.front();
            if (node)
            {
                mapNode->children.erase(
                    std::remove(mapNode->children.begin(), mapNode->children.end(), node),
                    mapNode->children.end());
                    
            }
            _removals.pop_front();
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
            if (attachment->referenceFrame == Attachment::ReferenceFrame::Relative)
            {
                if (attachment->horizonCulling)
                {
                    if (!obj->horizoncull)
                    {
                        obj->horizoncull = HorizonCullGroup::create();
                        obj->xform->addChild(attachment->node);
                    }
                    obj->horizoncull->addChild(attachment->node);
                }
                else
                {
                    obj->xform->addChild(attachment->node);
                }
            }
            else
            {
                obj->root->addChild(attachment->node);
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
        _additions.emplace_back(util::job::dispatch(compileNode));
    }
}

void
Application::remove(shared_ptr<MapObject> obj)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(obj, void());

    std::scoped_lock(_add_remove_mutex);
    _removals.push_back(obj->root);
}

namespace
{
    // generates a unique ID for each map object
    std::atomic_uint32_t uid_generator(0U);
}

MapObject::MapObject() :
    super(),
    uid(uid_generator++)
{
    root = vsg::Group::create();

    xform = GeoTransform::create();
    root->addChild(xform);
}

MapObject::MapObject(shared_ptr<Attachment> value) :
    MapObject()
{
    attachments = { value };
}

MapObject::MapObject(Attachments value) :
    MapObject()
{
    attachments = value;
}


LineString::LineString() :
    super()
{
    _geometry = LineStringGeometry::create();
    _bindStyle = BindLineStyle::create();
}

void
LineString::pushVertex(float x, float y, float z)
{
    _geometry->push_back({ x, y, z });
}

void
LineString::setStyle(const LineStyle& value)
{
    _bindStyle->setStyle(value);
}

const LineStyle&
LineString::style() const
{
    return _bindStyle->style();
}

void
LineString::createNode(Runtime& runtime)
{
    // TODO: simple approach. Just put everything in every LineString for now.
    // We can optimize or group things later.
    if (!node)
    {
        ROCKY_HARD_ASSERT(LineState::status.ok());
        auto stateGroup = vsg::StateGroup::create();
        stateGroup->stateCommands = LineState::pipelineStateCommands;
        stateGroup->addChild(_bindStyle);
        stateGroup->addChild(_geometry);
        node = stateGroup;
    }
}

JSON
LineString::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}



Mesh::Mesh() :
    super()
{
    _bindStyle = BindMeshStyle::create();
    _geometry = MeshGeometry::create();
}

void
Mesh::setStyle(const MeshStyle& value)
{
    _bindStyle->setStyle(value);
}

const MeshStyle&
Mesh::style() const
{
    return _bindStyle->style();
}

void
Mesh::createNode(Runtime& runtime)
{
    if (!node)
    {
        ROCKY_HARD_ASSERT(MeshState::status.ok());
        auto stateGroup = vsg::StateGroup::create();
        stateGroup->stateCommands = MeshState::pipelineStateCommands;
        stateGroup->addChild(_bindStyle);
        stateGroup->addChild(_geometry);
        node = stateGroup;
    }
}

JSON
Mesh::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}



Label::Label() :
    super()
{
    referenceFrame = ReferenceFrame::Relative;
    horizonCulling = true;
    _text = "Label!";
}

void
Label::setText(const std::string& value)
{
    _text = value;
    ROCKY_TODO();
}

const std::string&
Label::text() const
{
    return _text;
}

void
Label::createNode(Runtime& runtime)
{
    if (!_textNode)
    {
        const char* font_filename = ::getenv("ROCKY_DEFAULT_FONT");
        if (!font_filename) {
            Log::warn() << "No default font set in envvar ROCKY_DEFAULT_FONT" << std::endl;
            return;
        }

        auto font = vsg::read_cast<vsg::Font>(font_filename, runtime.readerWriterOptions);
        if (!font) {
            Log::warn() << "Cannot load font \"" << font_filename << "\"" << std::endl;
            return;
        }

        // NOTE: this will (later) happen in a LabelState class and only happen once.
        // In fact we will more likely create a custom shader/shaderset for text so we can do 
        // screen-space rendering and occlusion culling.
        {
            // assign a custom StateSet to options->shaderSets so that subsequent TextGroup::setup(0, options) call will pass in our custom ShaderSet.
            auto shaderSet = runtime.readerWriterOptions->shaderSets["text"] = vsg::createTextShaderSet(runtime.readerWriterOptions);
            auto depthStencilState = vsg::DepthStencilState::create();
            depthStencilState->depthTestEnable = VK_FALSE;
            shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
        }

        auto layout = vsg::StandardLayout::create();
        _textNode = vsg::Text::create();

        // currently vsg::GpuLayoutTechnique is the only technique that supports dynamic update of the text parameters
        _textNode->technique = vsg::GpuLayoutTechnique::create();

        const double size = 240000.0;
        layout->billboard = true;
        layout->billboardAutoScaleDistance = size;
        layout->position = vsg::vec3(0.0, 0.0, 0.0);
        layout->horizontal = vsg::vec3(size, 0.0, 0.0);
        layout->vertical = layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
        layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
        layout->outlineWidth = 0.1;
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->verticalAlignment = vsg::StandardLayout::BOTTOM_ALIGNMENT;

        _textNode->text = vsg::stringValue::create(_text);
        _textNode->font = font;
        _textNode->layout = layout;
        _textNode->setup(255, runtime.readerWriterOptions); // allocate enough space for max possible characters?

        auto h = HorizonCullGroup::create();
        h->addChild(_textNode);
        node = h;
        //node = _textNode;
    }
}


JSON
Label::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}