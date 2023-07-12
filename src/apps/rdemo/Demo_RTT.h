/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <vsg/all.h>
#include <rocky_vsg/engine/RTT.h>
#include <rocky_vsg/engine/Utils.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    vsg::ref_ptr<vsg::Node> load_rtt_model(const URI& uri, Runtime& runtime)
    {
        auto result = uri.read(IOOptions());
        if (result.status.ok())
        {
            // this is a bit awkward but it works when the URI has an extension
            auto options = vsg::Options::create(*runtime.readerWriterOptions);
            auto extension = std::filesystem::path(uri.full()).extension();
            options->extensionHint = extension.empty() ? std::filesystem::path(result.value.contentType) : extension;
            std::stringstream in(result.value.data);
            return vsg::read_cast<vsg::Node>(in, options);
        }
        else
        {
            Log::warn() << "FAILED TO LOAD MODEL." << std::endl;
            return vsg::Group::create();
        }
    }

    vsg::ref_ptr<vsg::Camera> make_rtt_camera(vsg::ref_ptr<vsg::Node> node)
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

        return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(VkExtent2D{512,512}));
    }
}

auto Demo_RTT = [](Application& app)
{
    static bool visible = true;
    static Status status;
    static shared_ptr<MapObject> object;
    static vsg::ref_ptr<vsg::View> view;
    static vsg::ref_ptr<vsg::ImageInfo> texture, depth;
    static vsg::ref_ptr<vsg::MatrixTransform> mt;
    static float rotation = 0.0f;

    if (!object)
    {
        // Find the main window and view:
        auto main = app.displayConfiguration.windows.begin();
        auto main_window = main->first;
        auto main_view = main->second.front();

        // this is the model we will see in the texture:
        URI uri("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");
        auto rtt_node = load_rtt_model(uri, app.instance.runtime());
        if (rtt_node)
        {
            mt = vsg::MatrixTransform::create();
            mt->addChild(rtt_node);
            rtt_node = mt;

            // RTT camera and view:
            auto rtt_cam = make_rtt_camera(rtt_node);
            auto rtt_view = vsg::View::create(rtt_cam, rtt_node);

            // and this is the render graph that will execute the RTT:
            auto vsg_context = vsg::Context::create(main_window->getOrCreateDevice());
            texture = vsg::ImageInfo::create();
            depth = vsg::ImageInfo::create();
            auto rtt_graph = RTT::createOffScreenRenderGraph(*vsg_context, { 512, 512 }, *texture, *depth);
            rtt_graph->addChild(rtt_view);
            app.addPreRenderGraph(main_window, rtt_graph);
        }

        // This is the geometry that we will apply the texture to. 
        // We have to add UVs (texture coordinates).
        auto mesh = Mesh::create();
        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double step = 2.5;
        const double alt = 50000;
        const double lon0 = -35.0, lon1 = 0.0, lat0 = -35.0, lat1 = 0.0;
        vsg::vec2 uv[4];
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
                    uv[i].s = (v[i].x - lon0) / (lon1 - lon0);
                    uv[i].t = (v[i].y - lat0) / (lat1 - lat0);
                    xform(v[i], v[i]);
                }

                mesh->addTriangle(v[0], v[1], v[2], uv[0], uv[1], uv[2]);
                mesh->addTriangle(v[0], v[2], v[3], uv[0], uv[2], uv[3]);
            }
        }
        mesh->setTexture(texture);
        mesh->setStyle(MeshStyle{ { 1,1,1,0.5 }, 64.0f });

        object = MapObject::create(mesh);
        app.add(object);

        // by the next frame, the object will be alive in the scene
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
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }
        ImGuiLTable::End();
    }
};
