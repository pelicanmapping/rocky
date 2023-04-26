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

// handy nice-looking table with names on the left.
namespace ImGuiLTable
{
    static bool Begin(const char* name)
    {
        bool ok = ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingFixedFit);
        if (ok) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
        }
        return ok;
    }

    static bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format=nullptr)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderFloat(s.c_str(), v, v_min, v_max, format);
    }

    static bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderFloat(s.c_str(), v, v_min, v_max, format, flags);
    }

    static bool SliderInt(const char* label, int* v, int v_min, int v_max)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderInt(s.c_str(), v, v_min, v_max);
    }

    static bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderInt(s.c_str(), v, v_min, v_max, format, flags);
    }

    static bool Checkbox(const char* label, bool* v)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::Checkbox(s.c_str(), v);
    }

    static bool BeginCombo(const char* label, const char* defaultItem)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::BeginCombo(s.c_str(), defaultItem);
    }

    static void EndCombo()
    {
        return ImGui::EndCombo();
    }

    static bool InputFloat(const char* label, float* v)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::InputFloat(s.c_str(), v);
    }

    static bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        return ImGui::ColorEdit3(label, col, flags);
    }

    template<typename...Args>
    static void Text(const char* label, const char* format, Args...args)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::Text(format, args...);
    }

    static void Section(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), label);
        ImGui::TableNextColumn();
    }

    static void End()
    {
        ImGui::EndTable();
    }
}



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

namespace
{
    auto Demo_LineString = [](Application& app)
    {
        static shared_ptr<LineString> line;
        static bool visible = true;

        if (!line)
        {
            ImGui::Text("Wait...");

            auto makeLine = [&app]() mutable
            {
                line = LineString::create();

                auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
                for (double lon = -180.0; lon <= 0.0; lon += 2.5)
                {
                    rocky::dvec3 ecef;
                    if (xform(rocky::dvec3(lon, 0.0, 6500000), ecef))
                        line->pushVertex(ecef);
                }

                line->setStyle(LineStyle{ { 1,1,0,1 }, 10.0f });

                app.add(MapObject::create(line));
            };

            app.instance.runtime().runDuringUpdate(makeLine);
            return;
        }

        if (ImGuiLTable::Begin("LineString"))
        {
            visible = line->visible;
            if (ImGuiLTable::Checkbox("Visible", &visible))
            {
                line->visible = visible;
            }

            LineStyle style = line->style();

            float* col = (float*)&style.color;
            if (ImGuiLTable::ColorEdit3("Color", col))
            {
                style.color.a = 1.0f;
                line->setStyle(style);
            }

            if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.1f"))
            {
                line->setStyle(style);
            }

            if (ImGuiLTable::SliderInt("Stipple pattern", &style.stipple_pattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
            {
                line->setStyle(style);
            }

            if (ImGuiLTable::SliderInt("Stipple factor", &style.stipple_factor, 1, 4))
            {
                line->setStyle(style);
            }

            ImGuiLTable::End();
        }
    };

    auto Demo_Polygon = [](Application& app)
    {
    };

    auto Demo_Icon = [](Application& app)
    {
    };

    auto Demo_Model = [](Application& app)
    {
    };
}

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
        } });
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
    if (ImGui::Begin("Welcome to Rocky."))
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
