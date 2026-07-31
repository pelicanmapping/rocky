/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/vsg/ecs/MotionSystem.h>

using namespace ROCKY_NAMESPACE;



struct PerspectiveFrustumView
{
    entt::entity e = entt::null;

    void init(entt::registry& r)
    {
        e = r.create();
        auto& frustum_transform = r.emplace<Transform>(e);
        frustum_transform.frustumCulled = false;
        auto& lineStyle = r.emplace<LineStyle>(e);
        lineStyle.color = StockColor::White;
        auto& lineGeom = r.emplace<LineGeometry>(e);
        lineGeom.topology = LineTopology::Segments;
        lineGeom.points.resize(24);
        auto& line = r.emplace<Line>(e, lineGeom, lineStyle);
    }

    void update(entt::registry& r, const Transform& hostTransform, const Optics& optics, const OpticsViewDetail& opticsDetail)
    {
        auto& frustum_xform = r.get<Transform>(e);
        auto& lineGeom = r.get<LineGeometry>(e);

        frustum_xform.position = hostTransform.position;
        frustum_xform.topocentric = hostTransform.topocentric;
        frustum_xform.localMatrix = hostTransform.localMatrix * optics.pose;
        frustum_xform.dirty(r);

        double nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
        double farDistance = optics.focalDistance * optics.farScale + optics.farBias;

        double nearClip = std::max(0.01, opticsDetail.nearDistance);
        double farClip = std::max(nearClip + 0.01, opticsDetail.farDistance);

        double tanHalfFovY = tan(glm::radians(optics.fovY * 0.5));
        double nearHalfH = nearClip * tanHalfFovY;
        double nearHalfW = nearHalfH * optics.aspectRatio;
        double farHalfH = farClip * tanHalfFovY;
        double farHalfW = farHalfH * optics.aspectRatio;

        glm::dvec3 eye(0, 0, 0);
        glm::dvec3 ntl(-nearHalfW, nearHalfH, -nearClip);
        glm::dvec3 ntr(nearHalfW, nearHalfH, -nearClip);
        glm::dvec3 nbl(-nearHalfW, -nearHalfH, -nearClip);
        glm::dvec3 nbr(nearHalfW, -nearHalfH, -nearClip);
        glm::dvec3 ftl(-farHalfW, farHalfH, -farClip);
        glm::dvec3 ftr(farHalfW, farHalfH, -farClip);
        glm::dvec3 fbl(-farHalfW, -farHalfH, -farClip);
        glm::dvec3 fbr(farHalfW, -farHalfH, -farClip);

        lineGeom.points = {
            eye, ftl, eye, ftr, eye, fbl, eye, fbr, // sides
            ntl, ntr, ntr, nbr, nbr, nbl, nbl, ntl, // near clip
            ftl, ftr, ftr, fbr, fbr, fbl, fbl, ftl  // far clip
        };
        lineGeom.dirty(r);
    }
};



auto Demo_Decal_Orthographic = [](Application& app)
{
    static entt::entity e_decal = entt::null;
    static double scale = 10000.0;

    if (e_decal == entt::null)
    {
        auto&& [_, reg] = app.registry.write();

        e_decal = reg.create();

        auto& xform = reg.emplace<Transform>(e_decal);
        xform.position = GeoPoint(SRS::WGS84, 0.0, 51.50, 0.0); // London, UK
        xform.topocentric = true;

        auto& optics = reg.emplace<Optics>(e_decal);
        optics.projection = Optics::Projection::Orthographic;
        optics.pose = glm::scale(glm::dmat4(1), glm::dvec3(scale));

        auto& decal = reg.emplace<Decal>(e_decal);
        decal.optics = e_decal;

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = xform.position;
            vp.range = scale * 10.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    auto&& [_, reg] = app.registry.read();
    if (ImGuiLTable::Begin("decal-ortho"))
    {
        ImGuiLTable::Checkbox("Show", &reg.get<Visibility>(e_decal).visible[0]);

        ImGuiLTable::End();
    }
};


auto Demo_Decal_Perspective = [](Application& app)
{
    static entt::entity e_decal = entt::null;
    static PerspectiveFrustumView frustumView;
    static double altitude = 10000.0;

    if (e_decal == entt::null)
    {
        auto&& [_, reg] = app.registry.write();

        e_decal = reg.create();

        auto& xform = reg.emplace<Transform>(e_decal);
        xform.position = GeoPoint(SRS::WGS84, 7.3606, 46.2331, altitude); // Sion, Switzerland
        xform.topocentric = true;

        auto& optics = reg.emplace<Optics>(e_decal);
        optics.projection = Optics::Projection::Perspective;
        optics.fovY = 45.0;
        optics.pose = glm::rotate(glm::dmat4(1), glm::radians(45.0), glm::dvec3(1, 0, 0));

        auto& style = reg.emplace<DecalStyle>(e_decal);
        style.color.a = 0.5f;

        reg.emplace<Decal>(e_decal);

        frustumView.init(reg);

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = xform.position;
            vp.range = altitude * 10.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    auto&& [_, reg] = app.registry.read();

    if (ImGuiLTable::Begin("decal-persp"))
    {
        ImGuiLTable::Checkbox("Show", &reg.get<Visibility>(e_decal).visible[0]);

        auto& [optics, opticsDetail, style] = reg.get<Optics, OpticsDetail, DecalStyle>(e_decal);

        if (ImGuiLTable::SliderFloat("Opacity", &style.color.a, 0.0f, 1.0f, "%.1f"))
        {
            style.dirty(reg);
        }

        auto quat = quaternion_from_unscaled_matrix<glm::dquat>(optics.pose);
        auto [xaxis, yaxis, zaxis] = euler_degrees_from_quaternion(quat);

        if (ImGuiLTable::SliderDouble("Heading", &zaxis, -180.0, 180.0, "%.1lf"))
        {
            optics.pose = glm::mat4_cast(quaternion_from_euler_degrees(xaxis, yaxis, zaxis));
        }

        if (ImGuiLTable::SliderDouble("Pitch", &xaxis, 0.0, 90.0, "%.1lf"))
        {
            optics.pose = glm::mat4_cast(quaternion_from_euler_degrees(xaxis, yaxis, zaxis));
        }

        ImGuiLTable::Checkbox("Auto compute near/far", &optics.autoComputeNearFar);

        ImGuiLTable::SliderDouble("Near scale", &optics.nearScale, 0.0, 1.0, "%.2lf");
        ImGuiLTable::SliderDouble("Near bias", &optics.nearBias, -100000.0, 0.0, "%.1lf", ImGuiSliderFlags_Logarithmic);
        ImGuiLTable::SliderDouble("Far scale", &optics.farScale, 1.0, 2.0, "%.2lf");
        ImGuiLTable::SliderDouble("Far bias", &optics.farBias, 0.0, 100000.0, "%.1lf", ImGuiSliderFlags_Logarithmic);
        
        ImGuiLTable::End();

        frustumView.update(reg, reg.get<Transform>(e_decal), optics, opticsDetail.views[0]);
    }
};



auto Demo_Decal_Projector = [](Application& app)
{
    static entt::entity e_platform = entt::null;
    static PerspectiveFrustumView frustumView;
    static CallbackSubs subs;

    if (e_platform == entt::null)
    {
        // load up a texture image
        auto& io = app.io();
        auto r = io.services().readImageFromURI(TEXTURE_GRID, io);
        Image::Ptr image;
        if (r.ok()) {
            image = r.value();
        }

        auto [_, reg] = app.registry.write();

        e_platform = reg.create();

        // a 3D model
        auto& model = reg.emplace<Model>(e_platform);
        model.uri = URI(MODEL_AIRPLANE);
        model.localMatrix = glm::rotate(glm::dmat4(1), glm::radians(90.0), glm::dvec3(0,0,1));

        // place and orient the model:
        auto& transform = reg.emplace<Transform>(e_platform);
        transform.position = GeoPoint(SRS::WGS84, -119.5, 34.0, 50000.0).transform(SRS::ECEF);
        transform.topocentric = true;

        // scale up the model so we can see it:
        auto& pixelScale = reg.emplace<PixelScale>(e_platform);
        pixelScale.minPixels = 256;
        pixelScale.maxPixels = FLT_MAX;

        // put the model in motion:
        auto& motion = reg.emplace<MotionGreatCircle>(e_platform);
        motion.normalAxis = SRS::WGS84.ellipsoid().rotationAxis(transform.position, 90.0);
        motion.velocity = glm::dvec3(2200, 0, 0);

        // define the projector's optical parameters:
        auto& optics = reg.emplace<Optics>(e_platform);
        optics.projection = Optics::Projection::Perspective;
        optics.fovY = 35.0f;
        optics.autoComputeFocalDistance = true;
        optics.autoComputeNearFar = true;
        
        // style the decal:
        auto& style = reg.emplace<DecalStyle>(e_platform);
        style.image = image;
        style.color.a = 0.5f;

        // the decal itself:
        reg.emplace<Decal>(e_platform);

        // A frustum view to visualize the optics and the projection:
        frustumView.init(reg);

        // A worker to animate the platform and its projector:
        MotionSystem motionsys(app.registry);
        double heading = 90.0, pitch = 25.0;
        auto animate = [&app, motionsys, heading, pitch](VSGContext) mutable
        {
            // update the platform's position based on its Motion component:
            motionsys.update(app.vsgcontext);

            auto&& [_, r] = app.registry.write();

            // pan the projector from left to right:
            heading = -90.0 + 10.0 * sin((double)(app.frameCount()) * 0.01);
            auto& [optics, opticsDetail] = r.get<Optics, OpticsDetail>(e_platform);
            optics.pose = glm::mat4_cast(quaternion_from_euler_degrees(pitch, 0.0, heading));

            // update the frustum geometry to represent a frustum view of the optics:
            auto& platform_xform = r.get<Transform>(e_platform);
            frustumView.update(r, platform_xform, optics, opticsDetail.views[0]);

            
            app.vsgcontext->requestFrame();
        };  
        subs += app.vsgcontext->onUpdate(animate);


        // zoom to the aoi:
        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = transform.position;
            vp.range = 300000.0;
            vp.pitch = -30.0;
            vp.heading = 45.0;
            manip->setViewpoint(vp);
        }
    }


    if (ImGuiLTable::Begin("decal-projector"))
    {
        static bool tether = false;
        if (ImGuiLTable::Checkbox("Tether", &tether))
        {
            if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
            {
                if (tether) {
                    Viewpoint vp = manip->viewpoint();
                    vp.pointFunction = [&]() {
                        auto&& [_, reg] = app.registry.read();
                        auto& xform = reg.get<Transform>(e_platform);
                        return xform.position;
                    };
                    vp.range = 200000.0;
                    manip->setViewpoint(vp);
                }
                else {
                    Viewpoint vp = manip->viewpoint();
                    vp.pointFunction = {};
                    manip->setViewpoint(vp);
                }
            }
        }

        ImGuiLTable::End();
    }
};





auto Demo_Decal_Stamper = [](Application& app)
{
    static std::vector<entt::entity> e_stamps;
    static entt::entity e_cursor = entt::null;
    static entt::entity e_style = entt::null;
    static double scale = 10000.0;
    static CallbackSubs subs;
    static bool cursor = true;
    static bool stamping = false;

    if (e_cursor == entt::null)
    {
        // load up a texture image
        Image::Ptr image;
        auto& io = app.io();
        auto r = io.services().readImageFromURI(TEXTURE_GRID, io);
        if (r.ok())
            image = r.value();

        auto [_, reg] = app.registry.write();

        // create a decal style that uses the texture image
        e_style = reg.create();
        auto& style = reg.emplace<DecalStyle>(e_style);
        style.image = image;

        // Create our decal. We can assign the style to it later.
        e_cursor = reg.create();

        auto& optics = reg.emplace<Optics>(e_cursor);
        optics.projection = Optics::Projection::Orthographic;

        auto& decal = reg.emplace<Decal>(e_cursor);

        // A transform component to place and move it on the map
        auto& transform = reg.emplace<Transform>(e_cursor);
        transform.position = GeoPoint(SRS::WGS84, 0.0, 51.50, 0.0);
        transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
        transform.topocentric = true;

        // install mouse callback that will move the cursor with the mouse:
        auto onMouseMove = [&](const TerrainIntersection& i, const View& view)
        {
            if (cursor && i.point)
            {
                app.registry.write([&](entt::registry& r)
                {
                    auto& transform = r.get<Transform>(e_cursor);
                    transform.position = i.point;
                    transform.dirty(r);
                });
            }
        };

        auto onMouseClick = [&](const TerrainIntersection& i, const View& view)
        {
            if (stamping && i.point)
            {
                auto&& [_, reg] = app.registry.write();

                auto e = reg.create();
                auto& optics = reg.emplace<Optics>(e);
                auto& decal = reg.emplace<Decal>(e, e_style);
                auto& transform = reg.emplace<Transform>(e);
                transform.position = i.point;
                transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.topocentric = true;

                e_stamps.emplace_back(e);
            }
        };

        auto handler = app.viewer->getObject<GeoMouseHandler>("demo.mouse");
        subs += handler->onMouseMove(onMouseMove);
        subs += handler->onLeftClick(onMouseClick);
    }


    if (ImGuiLTable::Begin("decal"))
    {
        {
            auto reader = app.registry.read();
            auto& reg = reader.registry;
            auto& decal = reg.get<Decal>(e_cursor);

            auto& transform = reg.get<Transform>(e_cursor);

            auto rot = quaternion_from_matrix<glm::dquat>(transform.localMatrix);
            auto [pitch, roll, heading] = euler_degrees_from_quaternion(rot);

            if (ImGuiLTable::Checkbox("Show cursor", &cursor))
            {
                reg.get<Visibility>(e_cursor).visible = cursor;
            }

            ImGuiLTable::Checkbox("Click to stamp", &stamping);

            static bool useTexture = false;
            if (ImGuiLTable::Checkbox("Texture", &useTexture))
            {
                decal.style = useTexture ? e_style : entt::null;
                decal.dirty(reg);
            }

            if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
            {
                auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
            }

            if (ImGuiLTable::SliderDouble("Pitch", &pitch, -90.0, 90.0, "%.1lf"))
            {
                auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
            }

            if (ImGuiLTable::SliderDouble("Scale", &scale, 1.0, 100000.0, "%.1lf", ImGuiSliderFlags_Logarithmic))
            {
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
            }
        }

        if (ImGuiLTable::Button("Clear stamps"))
        {
            app.registry.write([&](entt::registry& r)
            {
                r.destroy(e_stamps.begin(), e_stamps.end());
                e_stamps.clear();
            });
        }

        ImGuiLTable::End();
    }
};
