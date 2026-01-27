/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <vsg/all.h>
#include <rocky/URI.h>
#include <rocky/vsg/RTT.h>
#include "helpers.h"
#include <filesystem>

using namespace ROCKY_NAMESPACE;

namespace
{
    // Loads an external model to display in the RTT scene
    vsg::ref_ptr<vsg::Node> load_rtt_model(const URI& uri, VSGContext& vsgcontext)
    {
        auto result = uri.read({});
        if (result.ok())
        {
            // this is a bit awkward but it works when the URI has an extension
            auto options = vsg::Options::create(*vsgcontext->readerWriterOptions);
            auto extension = std::filesystem::path(uri.full()).extension();
            options->extensionHint = extension.empty() ? std::filesystem::path(result.value().content.type) : extension;
            std::istringstream in(result.value().content.data);
            return vsg::read_cast<vsg::Node>(in, options);
        }
        else
        {
            return {};
        }
    }

    // Makes a VSG camera to render the RTT scene
    vsg::ref_ptr<vsg::Camera> make_rtt_camera(vsg::ref_ptr<vsg::Node> node, const VkExtent2D size)
    {
        vsg::ComputeBounds computeBounds;
        node->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double r = 0.7 * vsg::length(computeBounds.bounds.max - computeBounds.bounds.min);

        // set up the camera
        auto view = vsg::LookAt::create(vsg::dvec3{ 0, r, 0 }, vsg::dvec3{ 0, 0, 0 }, vsg::dvec3{ 0, 0, 1 });
        auto proj = vsg::Orthographic::create(-r, r, -r, r, -r*5, r*5);
        return vsg::Camera::create(proj, view, vsg::ViewportState::create(size));
    }
}

auto Demo_RTT = [](Application& app)
{
    static Status status;
    static entt::entity entity = entt::null;
    static vsg::ref_ptr<vsg::MatrixTransform> mt;
    static float rotation = 0.0f;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", status.error().message.c_str());
        return;
    }

    if (entity == entt::null)
    {
        // Find the main window and view:
        auto main_window = app.display.mainWindow();
        auto main_view = app.display.views(main_window).front();

        // Create a simple VSG model using the Builder.
        vsg::Builder builder;
        vsg::GeometryInfo gi;       
        gi.color = to_vsg(Color::Cyan);
        vsg::StateInfo si;
        si.lighting = false, si.wireframe = true;
        auto rtt_node = builder.createBox(gi, si);

        // Make a transform so we can spin the model.
        mt = vsg::MatrixTransform::create();
        mt->addChild(rtt_node);
        rtt_node = mt;

        // Set up the RTT camera and view.
        VkExtent2D size{ 256, 256 };
        auto rtt_cam = make_rtt_camera(rtt_node, size);
        auto rtt_view = vsg::View::create(rtt_cam, rtt_node);

        // This is the render graph that will execute the RTT.
        auto context = vsg::Context::create(main_window->getOrCreateDevice());
        auto texture = vsg::ImageInfo::create();
        auto depth = vsg::ImageInfo::create();
        auto rtt_graph = RTT::createOffScreenRenderGraph(*context, size, texture, depth);
        rtt_graph->addChild(rtt_view);

        // Add the RTT graph to our application's main window.
        // TODO: possibly replace this with the functionality described here:
        // https://github.com/vsg-dev/VulkanSceneGraph/discussions/928
        auto install = [&app, rtt_graph, main_window]()
            {
                auto commandGraph = app.display.commandGraph(main_window);
                if (commandGraph)
                {
                    // Insert the pre-render graph into the command graph and compile it.
                    // This seems a bit awkward but it works.
                    commandGraph->children.insert(commandGraph->children.begin(), rtt_graph);
                    app.display.compileRenderGraph(rtt_graph, main_window);
                }
            };
        app.onNextUpdate(install);


        // Now, create a Mesh entity to host our spinning object.
        auto [_, reg] = app.registry.write();

        entity = reg.create();

        // This is the geometry that we will apply the texture to.
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Generate the mesh geometry - We have to add UVs (texture coordinates)
        // in order to map the RTT texture.
        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double step = 2.5;
        const double alt = 500000;
        const double lon0 = -35.0, lon1 = 0.0, lat0 = -35.0, lat1 = 0.0;
        glm::vec2 uv[4];
        glm::vec4 bg{ 1,1,1,1 };
        for(double lon = lon0; lon < lon1; lon += step)
        {
            for(double lat = lat0; lat < lat1; lat += step)
            {
                glm::dvec3 v[4] = {
                    {lon, lat, alt},
                    {lon + step, lat, alt},
                    {lon + step, lat + step, alt},
                    {lon, lat + step, alt} };

                for (int i = 0; i < 4; ++i) {
                    uv[i].s = (float)((v[i].x - lon0) / (lon1 - lon0));
                    uv[i].t = (float)((v[i].y - lat0) / (lat1 - lat0));
                    xform(v[i], v[i]);
                }

                std::uint32_t i = (std::uint32_t)geom.vertices.size();
                geom.vertices.insert(geom.vertices.end(), { v[0], v[1], v[2], v[3] });
                geom.uvs.insert(geom.uvs.end(), { uv[0], uv[1], uv[2], uv[3] });
                geom.indices.insert(geom.indices.end(), { i, i + 1, i + 2, i, i + 2, i + 3 });
            }
        }

        // create a style
        auto& style = reg.emplace<MeshStyle>(entity);
        style.color = Color{ 1,1,1,0.5 };
        style.depthOffset = 64.0f;

        // add the texture
        style.texture = reg.create();
        auto& t = reg.emplace<MeshTexture>(style.texture);
        t.imageInfo = texture;

        // and finally create the Mesh itself.
        reg.emplace<Mesh>(entity, geom, style);

        app.vsgcontext->requestFrame();        
        return;
    }

    // spin the model a little each frame.
    if (mt)
    {
        mt->matrix = vsg::rotate(rotation, vsg::normalize(vsg::vec3(1, 1, 1)));
        rotation += 0.01f;
        app.vsgcontext->requestFrame(); // render continuously.
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto [_, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        ImGuiLTable::End();
    }
};
