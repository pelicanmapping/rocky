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

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>

using namespace ROCKY_NAMESPACE;

#include "Demo_LineString.h"
#include "Demo_Mesh.h"
#include "Demo_Icon.h"
#include "Demo_Model.h"
#include "Demo_Label.h"
#include "Demo_MapManipulator.h"

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
    std::function<void(Application&)> function;
    std::vector<Demo> children;
};
std::vector<Demo> demos;

void setup_demos(rocky::Application& app)
{
    demos.emplace_back(
        Demo{ "MapObjects", {},
        {
            Demo{ "Label", Demo_Label },
            Demo{ "LineString", Demo_LineString },
            Demo{ "Mesh", Demo_Mesh },
            Demo{ "Icon", Demo_Icon },
            Demo{ "Model", Demo_Model }
        } }
    );
    demos.emplace_back(
        Demo{ "Camera", {}, {
            Demo{ "MapManipulator", Demo_MapManipulator }
        } }
    );
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
        };
    }
};


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    rocky::Log::level = rocky::LogLevel::ALL;

    // add an imagery layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->setURI("https://readymap.org/readymap/tiles/1.0.0/7/");
    app.map()->layers().add(layer);
    if (layer->status().failed())
        return layerError(layer);

    // start up the gui
    setup_demos(app);
    app.createMainWindow(1920, 1080);
    app.viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
    auto imgui = vsgImGui::RenderImGui::create(app.mainWindow);
    imgui->addChild(MainGUI::create(app));
    app.root->addChild(imgui); // add to root so it will render last.

    // run until the user quits.
    return app.run();
}
