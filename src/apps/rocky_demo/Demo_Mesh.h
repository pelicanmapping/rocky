/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Mesh_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Make an entity to hold our mesh:
        entity = reg.create();

        // Attach a mesh component:
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Make some geometry.
        const double step = 2.5;
        const double alt = 0.0;
        const double min_lon = 0.0, max_lon = 35.0;
        const double min_lat = 15.0, max_lat = 35.0;

        // A transform from WGS84 to the world SRS:
        auto xform = SRS::WGS84.to(app.mapNode->srs());

        for (double lon = 0.0; lon < 35.0; lon += step)
        {
            for(double lat = 15.0; lat < 35.0; lat += step)
            {
                auto v1 = xform(glm::dvec3(lon, lat, alt));
                auto v2 = xform(glm::dvec3(lon + step, lat, alt));
                auto v3 = xform(glm::dvec3(lon + step, lat + step, alt));
                auto v4 = xform(glm::dvec3(lon, lat + step, alt));
                
                geom.triangles.emplace_back(Triangle{ {v1, v2, v3} });
                geom.triangles.emplace_back(Triangle{ {v1, v3, v4} });
            }
        }

        // Add a dynamic style that we can change at runtime.
        auto& style = reg.emplace<MeshStyle>(entity);
        style.color = Color{ 1, 0.4f, 0.1f, 0.5f };
        style.depthOffset = 10000.0f;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

        // Turn off depth buffer writes
        mesh.writeDepth = false;

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        app.registry.read([&](entt::registry& reg)
            {
                static bool visible = true;
                if (ImGuiLTable::Checkbox("Show", &visible))
                    setVisible(reg, entity, visible);

                auto& style = reg.get<MeshStyle>(entity);

                if (ImGuiLTable::ColorEdit4("Color", (float*)&style.color))
                    style.dirty(reg);

                if (ImGuiLTable::SliderFloat("Depth offset", &style.depthOffset, 0.0f, 10000.0f, "%.0f"))
                    style.dirty(reg);

                if (ImGuiLTable::Checkbox("Wireframe", &reg.get<Mesh>(entity).wireFrame))
                    reg.get<Mesh>(entity).dirty(reg);
            });

        if (ImGuiLTable::Button("Recreate"))
        {
            app.registry.write([&](entt::registry& reg)
                {
                    reg.destroy(entity);
                    entity = entt::null;
                });
        }

        ImGuiLTable::End();
    }
};



auto Demo_Mesh_Relative = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Create a new entity to host our mesh
        entity = reg.create();

        // Attach the new mesh:
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Make some geometry that will be relative to a geolocation:
        const double s = 250000.0;
        glm::dvec3 verts[8] = {
            { -s, -s, -s },
            {  s, -s, -s },
            {  s,  s, -s },
            { -s,  s, -s },
            { -s, -s,  s },
            {  s, -s,  s },
            {  s,  s,  s },
            { -s,  s,  s }
        };
        unsigned indices[48] = {
            0,3,2, 0,2,1, 4,5,6, 4,6,7,
            1,2,6, 1,6,5, 3,0,4, 3,4,7,
            0,1,5, 0,5,4, 2,3,7, 2,7,6
        };

        Color color{ 1, 0, 1, 0.85f };

        for (unsigned i = 0; i < 48; )
        {
            geom.triangles.emplace_back(Triangle{
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });

            if ((i % 6) == 0)
                color.r *= 0.8f, color.b *= 0.8f;
        }

        auto& mesh = reg.emplace<Mesh>(entity, geom);

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates. We then set the bound on the node
        // to control horizon culling a little better
        auto& xform = reg.emplace<Transform>(entity);
        xform.topocentric = true;
        xform.position = GeoPoint(SRS::WGS84, 24.0, 24.0, s * 3.0);
        xform.radius = s * sqrt(2);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& transform = reg.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};






auto Demo_Mesh_Textured = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Create a new entity to host our mesh
        entity = reg.create();

        // Make some geometry that will be relative to a geolocation:
        auto& geom = reg.emplace<MeshGeometry>(entity);
        const double s = 1000000.0;
        glm::vec3 verts[4] = { { -s, -s, 0 }, {  s, -s, 0 }, {  s,  s, 0 }, { -s,  s, 0 } };
        glm::vec2 uvs[4] = { {0,0}, {1,0}, {1,1}, {0,1} };
        unsigned indices[6] = { 0, 1, 2, 0, 2, 3 };
        Color color{ 1, 1, 1, 0.85f };
        for (unsigned i = 0; i < 6; i += 3)
        {
            geom.triangles.emplace_back(Triangle{
                {verts[indices[i + 0]], verts[indices[i + 1]], verts[indices[i + 2]]},
                {color, color, color},
                {uvs[indices[i + 0]], uvs[indices[i + 1]], uvs[indices[i + 2]]} });
        }

        // A texture:
        glm::vec4 corners[4] = { {1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1} };
        auto image = Image::create(Image::R8G8B8_UNORM, 64, 64);
        image->eachPixel([&](const Image::iterator& i)
            {
                auto c = glm::mix(
                    glm::mix(corners[0], corners[1], i.u()),
                    glm::mix(corners[2], corners[3], i.u()), i.v());
                image->write(c, i);
            });

        auto sampler = vsg::Sampler::create();
        sampler->maxLod = 5;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 4.0f;

        auto& tex = reg.emplace<MeshTexture>(entity);
        tex.imageInfo = vsg::ImageInfo::create(sampler, util::moveImageToVSG(image));

        // Make a style pointing to our texture:
        auto& style = reg.emplace<MeshStyle>(entity);
        style.texture = entity;

        // Finally, a Mesh object pulls it all together.
        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates. We then set the bound on the node
        // to control horizon culling a little better
        auto& xform = reg.emplace<Transform>(entity);
        xform.topocentric = true;
        xform.position = GeoPoint(SRS::WGS84, 24.0, -24.0, s * 3.0);
        xform.radius = s * sqrt(2);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Texture"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        ImGuiLTable::End();
    }
};


auto Demo_Mesh_Shared = [](Application& app)
{
    static std::array<entt::entity, 3> styles;
    static std::array<entt::entity, 3> geoms;
    static std::vector<entt::entity> entities;
    static bool visible = true;
    static bool regenerate = false;
    const unsigned count = 1000;

    if (regenerate)
    {
        app.registry.write([&](entt::registry& registry)
            {
                registry.destroy(entities.begin(), entities.end());
                entities.clear();
                regenerate = false;
            });
    }

    if (entities.empty())
    {
        auto [_, reg] = app.registry.write();

        const double size = 100000;

        // make a sampler and a couple of textures.
        auto sampler = vsg::Sampler::create();
        sampler->maxLod = 5;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 4.0f;

        // Textures:
        glm::vec4 corners[4] = { {1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1} };
        auto image0 = Image::create(Image::R8G8B8_UNORM, 64, 64);
        image0->eachPixel([&](const Image::iterator& i)
            {
                auto c = glm::mix(
                    glm::mix(corners[0], corners[1], i.u()),
                    glm::mix(corners[2], corners[3], i.u()), i.v());
                image0->write(c, i);
            });

        auto image1 = Image::create(Image::R8G8B8A8_UNORM, 64, 64);
        image1->eachPixel([&](const Image::iterator& i)
            {
                auto c = glm::mix(
                    glm::mix(corners[1], corners[2], i.u()),
                    glm::mix(corners[0], corners[3], i.u()), i.v());
                c.a = 0.5f + 0.5f * sin(i.u() * glm::two_pi<float>() * 8.0f) * sin(i.v() * glm::two_pi<float>() * 8.0f);
                image1->write(c, i);
            });

        styles[0] = entities.emplace_back(reg.create());
        auto& style0 = reg.emplace<MeshStyle>(styles[0]);
        style0.color = Color::Lime;

        styles[1] = entities.emplace_back(reg.create());
        auto& style1 = reg.emplace<MeshStyle>(styles[1]);
        auto& tex1 = reg.emplace<MeshTexture>(styles[1]);
        tex1.imageInfo = vsg::ImageInfo::create(sampler, util::moveImageToVSG(image0));
        style1.texture = styles[1];

        styles[2] = entities.emplace_back(reg.create());
        auto& style2 = reg.emplace<MeshStyle>(styles[2]);
        auto& tex2 = reg.emplace<MeshTexture>(styles[2]);
        tex2.imageInfo = vsg::ImageInfo::create(sampler, util::moveImageToVSG(image1));
        style2.texture = styles[2];

        // Create a few different line objects.
        geoms[0] = entities.emplace_back(reg.create());
        auto& square = reg.emplace<MeshGeometry>(geoms[0]);
        square.verts = { { -size, -size, 0.0 }, {size, -size, 0.0}, {size, size, 0.0}, {-size, size, 0.0} };
        square.colors = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
        square.indices = { 0,1,2, 0,2,3 };

        geoms[1] = entities.emplace_back(reg.create());
        auto& triangle = reg.emplace<MeshGeometry>(geoms[1]);
        triangle.verts = { {0, size, 0}, {size, -size, 0}, {-size, -size, 0} };
        triangle.uvs = { {0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} };
        triangle.indices = { 0, 2, 1 };

        geoms[2] = entities.emplace_back(reg.create());
        auto& circle = reg.emplace<MeshGeometry>(geoms[2]);
        const int circle_points = 32;
        circle.verts.reserve(circle_points);
        circle.uvs.reserve(circle_points);
        circle.indices.reserve((circle_points - 2) * 3);
        for (int i = 0; i <= circle_points; ++i) {
            double angle = (double)i / (double)circle_points * glm::two_pi<double>();
            circle.verts.emplace_back(cos(angle) * size, sin(angle) * size, 0.0);
            circle.uvs.emplace_back((float)(0.5 + 0.5 * cos(angle)), (float)(0.5 + 0.5 * sin(angle)));
            if (i >= 2) {
                circle.indices.emplace_back(0);
                circle.indices.emplace_back(i - 1);
                circle.indices.emplace_back(i);
            }
        }

        // Now create a bunch of entities, each of which shares one of the above objects.
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

        entities.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            auto e = entities.emplace_back(reg.create());

            reg.emplace<Mesh>(e,
                reg.get<MeshGeometry>(geoms[i % 3]),
                reg.get<MeshStyle>(styles[i % 3]));

            double lat = rand_unit(mt) * 170.0 - 85.0;
            double lon = rand_unit(mt) * 360.0 - 180.0;

            // Add a transform that will place the line on the map
            auto& transform = reg.emplace<Transform>(e);
            transform.topocentric = true;
            transform.position = GeoPoint(SRS::WGS84, lon, lat, 10000.0 * double((i%3)+1.0));
            transform.radius = size; // for culling

            // Decluttering object, just to prove that it works with shared geometies:
            auto& dc = reg.emplace<Declutter>(e);
            dc.rect = { -10, -10, 10, 10 };
            dc.priority = i % 3;
        }

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped("Sharing: %d Mesh instances share MeshStyle and MeshGeometry components, but each has its own Transform.", count);

    if (ImGuiLTable::Begin("instanced linestring"))
    {
        if (ImGuiLTable::Button("Regenerate"))
        {
            regenerate = true;
        }

        ImGuiLTable::End();
    }
};