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

#include <rocky_vsg/Application.h>
#include <rocky/Version.h>

#if defined(ROCKY_SUPPORTS_GDAL)
#include <gdal_version.h>
#include <rocky/GDALImageLayer.h>
#endif

#if defined(ROCKY_SUPPORTS_TMS)
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#endif

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>

ROCKY_ABOUT(imgui, IMGUI_VERSION)

using namespace ROCKY_NAMESPACE;

#include "Demo_Map.h"
#include "Demo_LineString.h"
#include "Demo_Mesh.h"
#include "Demo_Icon.h"
#include "Demo_Model.h"
#include "Demo_Label.h"
#include "Demo_LineFeatures.h"
#include "Demo_PolygonFeatures.h"
#include "Demo_MapManipulator.h"
#include "Demo_Tethering.h"
#include "Demo_Environment.h"
#include "Demo_Views.h"
#include "Demo_RTT.h"
#include "Demo_Stats.h"

template<class T>
int layerError(T layer)
{
    rocky::Log::warn() << "Problem with layer \"" <<
        layer->name() << "\" : " << layer->status().message << std::endl;
    return -1;
}

auto Demo_About = [&](Application& app)
{
    for (auto& about : rocky::Instance::about())
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
std::vector<Demo> demos;

void setup_demos(rocky::Application& app)
{
    demos.emplace_back(
        Demo{ "Map", Demo_Map });

    demos.emplace_back(
        Demo{ "MapObjects", {},
        {
            Demo{ "Label", Demo_Label },
            Demo{ "LineString - absolute", Demo_LineString_Absolute },
            Demo{ "LineString - relative", Demo_LineString_Relative },
            Demo{ "Mesh - absolute", Demo_Mesh_Absolute },
            Demo{ "Mesh - relative", Demo_Mesh_Relative },
            Demo{ "Icon", Demo_Icon },
            Demo{ "User Model", Demo_Model }
        } }
    );

    demos.emplace_back(
        Demo{ "Features", {},
        {
            Demo{ "Line features", Demo_LineFeatures },
            Demo{ "Polygon features", Demo_PolygonFeatures }
        } }
    );

    demos.emplace_back(
        Demo{ "Camera", {},
        {
            Demo{ "Manipulator", Demo_MapManipulator },
            Demo{ "Tethering", Demo_Tethering }
        } }
    );

    demos.emplace_back(
        Demo{ "Environment", Demo_Environment });    
    demos.emplace_back(
        Demo{ "RTT", Demo_RTT });
    demos.emplace_back(
        Demo{ "Windows & Views", Demo_Views });
    demos.emplace_back(
        Demo{ "Stats", Demo_Stats });
    demos.emplace_back(
        Demo{ "About", Demo_About });
}

struct MainGUI : public vsg::Inherit<vsg::Command, MainGUI>
{
    Application& app;
    MainGUI(Application& app_) : app(app_) { }

    void record(vsg::CommandBuffer& cb) const override
    {
        if (ImGui::Begin("Welcome to Rocky"))
        {
            for (auto& demo : demos)
            {
                render(demo);
            }
        }
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


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    rocky::Log::level = rocky::LogLevel::INFO;

    if (app.map()->layers().empty())
    {
#ifdef ROCKY_SUPPORTS_TMS
        // add an imagery layer to the map
        auto layer = rocky::TMSImageLayer::create();
        layer->setURI("https://readymap.org/readymap/tiles/1.0.0/7");
        app.map()->layers().add(layer);
        if (layer->status().failed())
            return layerError(layer);

        // add an elevation layer to the map
        auto elev = rocky::TMSElevationLayer::create();
        elev->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
        app.map()->layers().add(elev);
        if (elev->status().failed())
            return layerError(elev);

  #if 0 // test multi-layer compositing
        auto layer2 = rocky::TMSImageLayer::create();
        layer2->setName("Hawaii inset");
        layer2->setURI("https://readymap.org/readymap/tiles/1.0.0/79/");
        app.map()->layers().add(layer2);
        if (layer2->status().failed())
            return layerError(layer2);

        auto layer3 = rocky::TMSImageLayer::create();
        layer3->setName("Florida inset");
        layer3->setURI("https://readymap.org/readymap/tiles/1.0.0/73/");
        app.map()->layers().add(layer3);
        if (layer3->status().failed())
            return layerError(layer3);

        auto layer4 = rocky::TMSImageLayer::create();
        layer4->setName("Washington DC");
        layer4->setURI("https://readymap.org/readymap/tiles/1.0.0/98/");
        app.map()->layers().add(layer4);
        if (layer4->status().failed())
            return layerError(layer4);

        auto layer5 = rocky::TMSImageLayer::create();
        layer5->setName("Florida inset");
        layer5->setURI("https://readymap.org/readymap/tiles/1.0.0/82/");
        app.map()->layers().add(layer5);
        if (layer5->status().failed())
            return layerError(layer5);
  #endif
#endif

        JSON map_json = app.map()->to_json();
        rocky::util::writeToFile(rocky::json_pretty(map_json), "out.map.json");
    }

    // start up the gui
    setup_demos(app);

    app.viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());

    auto window = app.addWindow(vsg::WindowTraits::create(1920, 1080, "Main Window"));
    auto imgui = vsgImGui::RenderImGui::create(window.value());
    imgui->addChild(MainGUI::create(app));

    // ImGui likes to live under the main rendergraph, but outside the main view.
    // https://github.com/vsg-dev/vsgExamples/blob/master/examples/ui/vsgimgui_example/vsgimgui_example.cpp#L276
    auto main_view = app.displayConfiguration.windows[window.value()].front();
    auto view_data = app.viewData(main_view);
    view_data.parentRenderGraph->addChild(imgui);

    // run until the user quits.
    return app.run();
}
