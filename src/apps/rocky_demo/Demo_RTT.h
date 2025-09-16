/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <vsg/all.h>
#include <rocky/URI.h>
#include <rocky/vsg/RTT.h>
#include <rocky/vsg/ecs/MeshSystem.h>
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
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
            centre, vsg::dvec3(0.0, 0.0, 1.0));

        auto perspective = vsg::Perspective::create(45.0, 1.0, nearFarRatio* radius, radius * 10.0);

        return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(size));
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
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.error().message.c_str());
        return;
    }

    if (entity == entt::null)
    {
        // Find the main window and view:
        auto main_window = app.display.mainWindow();
        auto main_view = app.display.views(main_window).front();

        // this is the model we will see in the RTT:
        URI uri("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");
        auto rtt_node = load_rtt_model(uri, app.vsgcontext);
        if (!rtt_node)
        {
            status = Failure(Failure::ResourceUnavailable, "Unable to load the model from " + uri.full());
            return;
        }

        // Make a transform so we can spin the model.
        mt = vsg::MatrixTransform::create();
        mt->addChild(rtt_node);
        rtt_node = mt;

        // RTT camera and view:
        VkExtent2D size{ 256, 256 };
        auto rtt_cam = make_rtt_camera(rtt_node, size);
        auto rtt_view = vsg::View::create(rtt_cam, rtt_node);

        // This is the render graph that will execute the RTT:
        auto vsg_context = vsg::Context::create(main_window->getOrCreateDevice());
        auto texture = vsg::ImageInfo::create();
        auto depth = vsg::ImageInfo::create();
        auto rtt_graph = RTT::createOffScreenRenderGraph(*vsg_context, size, texture, depth);
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

        auto [lock, registry] = app.registry.write();

        // Now, create an entity to host our mesh.
        // This is the geometry that we will apply the texture to. 
        entity = registry.create();
        auto& mesh = registry.emplace<Mesh>(entity);

        // Cenerate the mesh geometry We have to add UVs (texture coordinates)
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

                mesh.triangles.emplace_back(Triangle{ {v[0], v[1], v[2]}, { bg,bg,bg }, {uv[0], uv[1], uv[2]} });
                mesh.triangles.emplace_back(Triangle{ {v[0], v[2], v[3]}, { bg,bg,bg }, {uv[0], uv[2], uv[3]} });
            }
        }

        mesh.style = MeshStyle{ { 1,1,1,0.5 }, 64.0f };

        // add the texture
        mesh.texture = registry.create();
        auto& t = registry.emplace<Texture>(mesh.texture);
        t.imageInfo = texture;

        app.vsgcontext->requestFrame();
        
        return;
    }

    // spin the model.
    if (mt)
    {
        mt->matrix = vsg::rotate(rotation, vsg::vec3(1, 1, 1));
        rotation += 0.01f;
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto [lock, registry] = app.registry.read();

        auto& v = registry.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(registry, entity, v);

        ImGuiLTable::End();
    }
};
