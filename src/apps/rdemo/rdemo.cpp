/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky_vsg/Application.h>

#if !defined(ROCKY_SUPPORTS_TMS)
#error This example requires TMS support. Check CMake.
#endif

#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#include <rocky_vsg/Utils.h>

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>

using namespace ROCKY_NAMESPACE;

template<class T>
int layerError(T layer)
{
    rocky::Log::warn() << "Problem with layer \"" <<
        layer->name() << "\" : " << layer->status().message << std::endl;
    return -1;
}

struct Demo
{
    std::string name;
    std::function<void(Application& app)> function;
    std::vector<Demo> children;
};

#include "Demo_LineString.h"
#include "Demo_Polygon.h"
#include "Demo_Icon.h"
#include "Demo_Model.h"
#include "Demo_MapManipulator.h"

std::vector<Demo> demos;
Application* s_app;

void setup_demos(rocky::Application& app)
{
    s_app = &app;
    demos.emplace_back(
        Demo{ "Geometry", nullptr,
        {
            Demo{"LineString", Demo_LineString},
            Demo{"Polygon", Demo_Polygon },
            Demo{"Icon", Demo_Icon },
            Demo{"Model", Demo_Model }
        } }
    );
    demos.emplace_back(
        Demo{ "Controls", nullptr,
        {
            Demo{"MapManipulator", Demo_MapManipulator}
        } }
    );
}


void render(const Demo& demo, Application& app)
{
    if (ImGui::CollapsingHeader(demo.name.c_str()))
    {
        if (demo.function)
            demo.function(app);

        if (!demo.children.empty())
        {
            ImGui::Indent();
            for (auto& child : demo.children)
                render(child, app);
            ImGui::Unindent();
        }
    };
}

auto mainGUI = [&]()
{
    if (ImGui::Begin("Welcome to Rocky"))
    {
        for (auto& demo : demos)
            render(demo, *s_app);
        ImGui::End();
    }
    return true;
};


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    // add an imagery layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->setURI("https://readymap.org/readymap/tiles/1.0.0/7/");
    app.map()->layers().add(layer);

    // check for error
    if (layer->status().failed())
        return layerError(layer);

    // start up the gui
    setup_demos(app);
    app.createMainWindow(1920, 1080);
    app.viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());

    // ImGui must record last (after the main scene) so add it to root.
    app.root->addChild(vsgImGui::RenderImGui::create(app.mainWindow, mainGUI));

    // run until the user quits.
    return app.run();
}
