/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

/**
* THE DEMO APPLICATION is an ImGui-based app that shows off all the features
* of the Rocky Application API. We intend each "Demo_*" include file to be
* both a unit test for that feature, and a reference or writing your own code.
*/

#include <rocky/vsg/Application.h>
#include <rocky/Version.h>

#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>

#include <rocky/vsg/imgui/RenderImGui.h>
#include <rocky/vsg/imgui/SendEventsToImGui.h>

#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

ROCKY_ABOUT(imgui, IMGUI_VERSION)

using namespace ROCKY_NAMESPACE;

#include "Demo_Map.h"
#include "Demo_Line.h"
#include "Demo_Mesh.h"
#include "Demo_Icon.h"
#include "Demo_Model.h"
#include "Demo_Label.h"
#include "Demo_Widget.h"
#include "Demo_LineFeatures.h"
#include "Demo_PolygonFeatures.h"
#include "Demo_LabelFeatures.h"
#include "Demo_MapManipulator.h"
#include "Demo_Serialization.h"
#include "Demo_Tethering.h"
#include "Demo_Environment.h"
#include "Demo_Views.h"
#include "Demo_RTT.h"
#include "Demo_Stats.h"
#include "Demo_Geocoder.h"
#include "Demo_Rendering.h"
#include "Demo_Simulation.h"
#include "Demo_TrackHistory.h"
#include "Demo_Decluttering.h"

template<class T>
int layerError(T layer)
{
    rocky::Log()->warn("Problem with layer \"" + layer->name() + "\" : " + layer->status().message);
    return -1;
}

auto Demo_About = [](Application& app)
{
    for (auto& about : rocky::ContextImpl::about())
    {
        ImGui::Text(about.c_str());
    }
};

struct Demo
{
    std::string name;
    std::function<void(Application&)> function;
    std::vector<Demo> children;
};

std::vector<Demo> demos =
{
    Demo{ "Map", Demo_Map },
    Demo{ "Basic Components", {},
    {
        Demo{ "Label", Demo_Label },
        Demo{ "Line - absolute", Demo_Line_Absolute },
        Demo{ "Line - relative", Demo_Line_Relative },
        Demo{ "Mesh - absolute", Demo_Mesh_Absolute },
        Demo{ "Mesh - relative", Demo_Mesh_Relative },
        Demo{ "Icon", Demo_Icon },
        Demo{ "Model", Demo_Model },
        Demo{ "Widget", Demo_Widget }
    } },
    Demo{ "GIS Data", {},
    {
        Demo{ "Polygon features", Demo_PolygonFeatures },
        Demo{ "Line features", Demo_LineFeatures },
        Demo{ "Labels from features", Demo_LabelFeatures }
    } },
    Demo{ "Simulation", {},
    {
        Demo{ "Simulated platforms", Demo_Simulation },
        Demo{ "Track histories", Demo_TrackHistory }
    } },
    Demo{ "Decluttering", Demo_Decluttering },
    Demo{ "Geocoding", Demo_Geocoder },
    Demo{ "RTT", Demo_RTT },
    Demo{ "Camera", {},
    {
        Demo{ "Viewpoints", Demo_Viewpoints },
        Demo{ "Tethering", Demo_Tethering }
    } },
    Demo{ "Rendering", Demo_Rendering },
    Demo{ "Views", Demo_Views },
    Demo{ "Environment", Demo_Environment },
    Demo{ "Serialization", Demo_Serialization },
    Demo{ "Stats", Demo_Stats },
    Demo{ "About", Demo_About }
};

struct MainGUI : public vsg::Inherit<vsg::Command, MainGUI>
{
    Application& app;
    MainGUI(Application& app_) : app(app_) { }

    void record(vsg::CommandBuffer& cb) const override
    {
        render();
    }

    void render() const
    {
        ImGui::Begin("Welcome to Rocky");
        {
            for (auto& demo : demos)
            {
                render(demo);
            }
        }
        ImGui::End();
    }

    void render(const Demo& demo) const
    {
        if (ImGui::CollapsingHeader(demo.name.c_str()))
        {
            if (demo.function)
                demo.function(app);
    
            if (!demo.children.empty())
            {
                ImGui::Indent();
                for (auto& child : demo.children)
                    render(child);
                ImGui::Unindent();
            }
        }
    }
};

//! Wrapper to render via callbacks.
struct GuiCallbackRunner : public vsg::Inherit<vsg::Node, GuiCallbackRunner>
{
    VSGContext context;

    GuiCallbackRunner(VSGContext context_in) : context(context_in) { }

    void traverse(vsg::RecordTraversal& record) const override
    {
        for (auto& callback : context->guiCallbacks)
        {
            callback(record.getState()->_commandBuffer->viewID, ImGui::GetCurrentContext());
        }
    }
};

//! wrapper for vsgImGui::SendEventsToImGui that restricts ImGui events to a single window.
class SendEventsToImGuiWrapper : public vsg::Inherit<vsgImGui::SendEventsToImGui, SendEventsToImGuiWrapper>
{
public:
    SendEventsToImGuiWrapper(vsg::ref_ptr<vsg::Window> window, ImGuiContext* imguiContext, rocky::VSGContext& cx) :
        _window(window), _imguiContext(imguiContext), _context(cx) { }

    template<typename E>
    void propagate(E& e, bool forceRefresh = false)
    {
        if (!e.handled && e.window.ref_ptr() == _window)
        {
            ImGui::SetCurrentContext(_imguiContext);

            vsgImGui::SendEventsToImGui::apply(e);
            if (e.handled || forceRefresh)
            {
                _context->requestFrame();
            }
        }
    }

    void apply(vsg::ButtonPressEvent& e) override { propagate(e); }
    void apply(vsg::ButtonReleaseEvent& e) override { propagate(e); }
    void apply(vsg::ScrollWheelEvent& e) override { propagate(e); }
    void apply(vsg::KeyPressEvent& e) override { propagate(e); }
    void apply(vsg::KeyReleaseEvent& e) override { propagate(e); }
    void apply(vsg::MoveEvent& e) override { propagate(e); }
    void apply(vsg::ConfigureWindowEvent& e) override { propagate(e, true); }

private:
    vsg::ref_ptr<vsg::Window> _window;
    VSGContext _context;
    ImGuiContext* _imguiContext;
};


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    // Exit if the user tries to load a file and failed:
    if (app.commandLineStatus.failed())
    {
        Log()->error(app.commandLineStatus.toString());
        exit(-1);
    }

    // Add some default layers if the user didn't load a file:
    auto& layers = app.mapNode->map->layers();
    if (layers.empty())
    {
        auto imagery = rocky::TMSImageLayer::create();
        imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        layers.add(imagery);

        auto elevation = rocky::TMSElevationLayer::create();
        elevation->uri = "https://readymap.org/readymap/tiles/1.0.0/116/";
        layers.add(elevation);
    }

    // Create the main window and the main GUI:
    auto main_window = app.displayManager->addWindow(vsg::WindowTraits::create(1920, 1080, "Main Window"));
    auto main_imgui = vsgImGui::RenderImGui::create(main_window);

    // This imgui renderer is for in-scene widgets.
    auto scene_imgui = vsgImGui::RenderImGui::create(main_window);
    scene_imgui->addChild(GuiCallbackRunner::create(app.context));

    // Hook in any embedded GUI renderers:
    //main_imgui->addChild(GuiCallbackRunner::create(app.context));

    // Hook in the main demo gui:
    auto demo_gui = MainGUI::create(app);
    main_imgui->addChild(demo_gui);

    // ImGui likes to live under the main rendergraph, but outside the main view.
    // https://github.com/vsg-dev/vsgExamples/blob/master/examples/ui/vsgimgui_example/vsgimgui_example.cpp#L276
    auto main_view = app.displayManager->windowsAndViews[main_window].front();
    app.displayManager->getRenderGraph(main_view)->addChild(scene_imgui);
    app.displayManager->getRenderGraph(main_view)->addChild(main_imgui);

    // Make sure ImGui is the first event handler:
    auto& handlers = app.viewer->getEventHandlers();
    handlers.insert(handlers.begin(), SendEventsToImGuiWrapper::create(main_window, scene_imgui->context(), app.context));
    handlers.insert(handlers.begin(), SendEventsToImGuiWrapper::create(main_window, main_imgui->context(), app.context));

    // In render-on-demand mode, this callback will cause ImGui to handle events
    app.noRenderFunction = [&]()
        {
            // "render" each imgui context without calling ImGui::Render()
            // so we can still process events.
            ImGui::SetCurrentContext(scene_imgui->context());
            ImGui::NewFrame();

            for (auto& render : app.context->guiCallbacks)
                for (auto& viewID : app.context->activeViewIDs)
                    render(viewID, scene_imgui->context());

            ImGui::EndFrame();


            ImGui::SetCurrentContext(main_imgui->context());
            ImGui::NewFrame();

            demo_gui->render();

            ImGui::EndFrame();
        };

    // run until the user quits.
    return app.run();
}
