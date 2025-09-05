/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

/**
* THE DEMO APPLICATION is an ImGui-based app that shows off all the features
* of the Rocky Application API. We intend each "Demo_*" include file to be
* both a unit test for that feature, and a reference for writing your own code.
*/

#include <rocky/rocky.h>
#include <rocky/rocky_imgui.h>

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
#include "Demo_Environment.h"
#include "Demo_Views.h"
#include "Demo_RTT.h"
#include "Demo_Stats.h"
#include "Demo_Geocoder.h"
#include "Demo_Terrain.h"
#include "Demo_Simulation.h"
#include "Demo_TrackHistory.h"
#include "Demo_Decluttering.h"
#include "Demo_NodePager.h"
#include "Demo_FeatureView.h"
#include "Demo_MVTFeatures.h"
#include "Demo_DrawComponent.h"
#include "Demo_ElevationSampler.h"

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
    Demo{ "Basics", {},
    {
        Demo{ "Line - absolute", Demo_Line_Absolute },
        Demo{ "Line - reference", Demo_Line_ReferencePoint },
        Demo{ "Line - relative", Demo_Line_Relative },
        Demo{ "Mesh - absolute", Demo_Mesh_Absolute },
        Demo{ "Mesh - relative", Demo_Mesh_Relative },
        Demo{ "Icon", Demo_Icon },
        Demo{ "Model", Demo_Model },
        Demo{ "Widget", Demo_Widget },
        Demo{ "Label (old)", Demo_Label },
        Demo{ "Drawing", Demo_Draw },
        Demo{ "Node Pager", Demo_NodePager },
        Demo{ "RTT", Demo_RTT }
    } },
    Demo{ "GIS Data", {},
    {
        Demo{ "FeatureView", Demo_FeatureView },
        Demo{ "Polygon features", Demo_PolygonFeatures },
        Demo{ "Line features", Demo_LineFeatures },
        Demo{ "Labels from features", Demo_LabelFeatures },
        Demo{ "Vector tiles", Demo_MVTFeatures }
    } },
    Demo{ "Simulation", {},
    {
        Demo{ "Simulated platforms", Demo_Simulation },
        Demo{ "Track histories", Demo_TrackHistory }
    } },
    Demo{ "Decluttering", Demo_Decluttering },
    Demo{ "Elevation Queries", Demo_ElevationSampler },
    Demo{ "Geocoding", Demo_Geocoder },
    Demo{ "Camera", Demo_MapManipulator },
    Demo{ "Views", Demo_Views },
    Demo{ "Environment", Demo_Environment },
    Demo{ "Serialization", Demo_Serialization },
    Demo{ "Terrain", Demo_Terrain },
    Demo{ "Stats", Demo_Stats },
    Demo{ "About", Demo_About }
};

struct MainGUI : public vsg::Inherit<ImGuiContextNode, MainGUI>
{
    Application& app;
    MainGUI(Application& app_) : app(app_) { }
    mutable ImVec2 asize;
    mutable std::optional<std::string> attribution;

    void render(ImGuiContext* imguiContext) const override
    {
        ImGui::SetCurrentContext(imguiContext);
        ImGui::Begin("Welcome to Rocky");
        {
            for (auto& demo : demos)
            {
                render(demo);
            }
        }
        ImGui::End();

        renderAttribution();
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

    void renderAttribution() const
    {
        // Map attribution
        if (!attribution.has_value())
        {
            std::string buf;
            app.mapNode->map->each([&](auto layer)
                {
                    if (layer->status().ok() && layer->attribution.has_value()) {
                        if (!buf.empty())
                            buf += " | ";
                        buf += layer->attribution->text;
                    }
                });
            attribution = buf;
        }

        if (!attribution->empty())
        {
            auto winsize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(winsize.x - asize.x, winsize.y - asize.y));
            ImGui::SetNextWindowBgAlpha(0.65f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##Attribution", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
            ImGui::Text("%s", attribution->c_str());
            asize = ImGui::GetWindowSize();
            ImGui::End();
            ImGui::PopStyleVar(1);
        }
    }
};


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    // Exit if the user tries to load a file and failed:
    if (app.commandLineStatus.failed())
    {
        Log()->error(app.commandLineStatus.error().string());
        exit(-1);
    }

    // Add some default layers if the user didn't load a file:
    auto map = app.mapNode->map;

    if (map->layers().empty())
    {
        auto imagery = rocky::TMSImageLayer::create();
        imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        imagery->attribution = { "ReadyMap(R) data courtesy of Pelican Mapping" };
        map->add(imagery);

        auto elevation = rocky::TMSElevationLayer::create();
        elevation->uri = "https://readymap.org/readymap/tiles/1.0.0/116/";
        map->add(elevation);
    }

    // Create the main window:
    auto traits = vsg::WindowTraits::create(1920, 1080, "Main Window");
    auto main_window = app.display.addWindow(traits);

    // Add our GUI:
    auto imguiRenderer = RenderImGuiContext::create(main_window);
    imguiRenderer->add(MainGUI::create(app));
    app.install(imguiRenderer);

    // run until the user quits.
    return app.run();
}
