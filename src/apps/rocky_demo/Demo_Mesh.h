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
                
                std::uint32_t i = (std::uint32_t)geom.vertices.size();
                geom.vertices.insert(geom.vertices.end(), { v1, v2, v3, v4 });
                geom.indices.insert(geom.indices.end(), { i, i + 1, i + 2, i, i + 2, i + 3 });
            }
        }

        // Add a dynamic style that we can change at runtime.
        auto& style = reg.emplace<MeshStyle>(entity);
        style.color = Color{ 1, 0.4f, 0.1f, 0.5f };
        style.depthOffset = 10000.0f;
        style.writeDepth = false;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

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

                if (ImGuiLTable::Checkbox("Wireframe", &style.wireframe))
                    style.dirty(reg);
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

        glm::dvec3 vertices[8] = {
            { -s, -s, -s }, {  s, -s, -s },
            {  s,  s, -s }, { -s,  s, -s },
            { -s, -s,  s }, {  s, -s,  s },
            {  s,  s,  s }, { -s,  s,  s }
        };

        unsigned indices[48] = {
            0,3,2, 0,2,1, 4,5,6, 4,6,7,
            1,2,6, 1,6,5, 3,0,4, 3,4,7,
            0,1,5, 0,5,4, 2,3,7, 2,7,6
        };

        Color color{ 1, 0, 1, 0.85f };
        geom.vertices.reserve(12 * 3);
        geom.colors.reserve(12 * 3);

        for (int i = 0; i < 48; )
        {
            for (int v = 0; v < 3; ++v)
            {
                geom.indices.emplace_back(i);
                geom.vertices.emplace_back(vertices[indices[i++]]);
                geom.colors.emplace_back(color);
            }

            if ((i % 6) == 0)
                color.r *= 0.8f, color.b *= 0.8f;
        }

        // Set up our style to use the embedded colors.
        auto& style = reg.emplace<MeshStyle>(entity);
        style.useGeometryColors = true;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

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

        auto& style = reg.get<MeshStyle>(entity);
        if (ImGuiLTable::Checkbox("Wireframe", &style.wireframe))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Draw backfaces", &style.drawBackfaces))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Depth write", &style.writeDepth))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Two-pass alpha", &style.twoPassAlpha)) {
            if (style.twoPassAlpha)
                style.writeDepth = true;
            style.dirty(reg);
        }

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
        geom.vertices = { { -s, -s, 0 }, {  s, -s, 0 }, {  s,  s, 0 }, { -s,  s, 0 } };
        geom.uvs = { {0,0}, {1,0}, {1,1}, {0,1} };
        geom.indices = { 0, 1, 2, 0, 2, 3 };

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

        // a sampler:
        auto sampler = vsg::Sampler::create();
        sampler->maxLod = 5;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 4.0f;

        // Make a texture component to hold our image:
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
        xform.position = GeoPoint(SRS::WGS84, 24.0, -24.0, s);
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
    const unsigned count = 100000;

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

        // First texture:
        glm::vec4 corners[4] = { {1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1} };
        auto image0 = Image::create(Image::R8G8B8_UNORM, 64, 64);
        image0->eachPixel([&](const Image::iterator& i)
            {
                auto c = glm::mix(
                    glm::mix(corners[0], corners[1], i.u()),
                    glm::mix(corners[2], corners[3], i.u()), i.v());
                image0->write(c, i);
            });

        // Second texture:
        auto image1 = Image::create(Image::R8G8B8A8_UNORM, 64, 64);
        image1->eachPixel([&](const Image::iterator& i)
            {
                auto c = glm::mix(
                    glm::mix(corners[1], corners[2], i.u()),
                    glm::mix(corners[0], corners[3], i.u()), i.v());
                c.a = 0.5f + 0.5f * sin(i.u() * glm::two_pi<float>() * 8.0f) * sin(i.v() * glm::two_pi<float>() * 8.0f);
                image1->write(c, i);
            });

        // Create a few different styles.
        styles[0] = entities.emplace_back(reg.create());
        auto& style0 = reg.emplace<MeshStyle>(styles[0]);
        style0.color = Color::Red;

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

        // Create a few different geometries.
        geoms[0] = entities.emplace_back(reg.create());
        auto& square = reg.emplace<MeshGeometry>(geoms[0]);
        square.vertices = { { -size, -size, 0.0 }, {size, -size, 0.0}, {size, size, 0.0}, {-size, size, 0.0} };
        square.colors = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
        square.indices = { 0,1,2, 0,2,3 };

        geoms[1] = entities.emplace_back(reg.create());
        auto& triangle = reg.emplace<MeshGeometry>(geoms[1]);
        triangle.vertices = { {0, size, 0}, {size, -size, 0}, {-size, -size, 0} };
        triangle.uvs = { {0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} };
        triangle.indices = { 0, 2, 1 };

        geoms[2] = entities.emplace_back(reg.create());
        auto& circle = reg.emplace<MeshGeometry>(geoms[2]);
        const int circle_points = 32;
        circle.vertices.reserve(circle_points);
        circle.uvs.reserve(circle_points);
        circle.indices.reserve((circle_points - 2) * 3);
        for (int i = 0; i <= circle_points; ++i) {
            double angle = (double)i / (double)circle_points * glm::two_pi<double>();
            circle.vertices.emplace_back(cos(angle) * size, sin(angle) * size, 0.0);
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
            transform.position = GeoPoint(SRS::WGS84, lon, lat, 5000.0 + (double(i) * 100.0));
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





auto Demo_Mesh_Blending = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Create a new entity to host our mesh
        entity = reg.create();

        // Attach the new mesh:
        auto& geom = reg.emplace<MeshGeometry>(entity);

        geom.vertices = {
            { -200000.0, -200000.0, 200000.0 },
            {  300000.0, -200000.0, 200000.0 },
            {  300000.0,  300000.0, 200000.0 },
            { -200000.0,  300000.0, 200000.0 },

            { -250000.0, -250000.0, 0.0 },
            {  250000.0, -250000.0, 0.0 },
            {  250000.0,  250000.0, 0.0 },
            { -250000.0,  250000.0, 0.0 },
                
            { -225000.0, -225000.0, 100000.0 },
            {  275000.0, -225000.0, 100000.0 },
            {  275000.0,  275000.0, 100000.0 },
            { -225000.0,  275000.0, 100000.0 } };

        const float a = 0.5f;

        geom.colors = {
            // color each pair of triangles a different primary color, with a constant alpha of 0.75:
            {1,0,0,a}, {1,0,0,a}, {1,0,0,a}, {1,0,0,a},
            {0,1,0,a}, {0,1,0,a}, {0,1,0,a}, {0,1,0,a},
            {0,0,1,a}, {0,0,1,a}, {0,0,1,a}, {0,0,1,a} };

        geom.indices = {
            0,1,2, 0,2,3,
            4,5,6, 4,6,7,
            8,9,10, 8,10,11
        };

        auto& style = reg.emplace<MeshStyle>(entity);
        style.twoPassAlpha = true;
        style.writeDepth = true;
        style.useGeometryColors = true;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates. We then set the bound on the node
        // to control horizon culling a little better
        auto& xform = reg.emplace<Transform>(entity);
        xform.topocentric = true;
        xform.position = GeoPoint(SRS::WGS84, 0.0, 0.0, 10000.0);
        xform.radius = 40000.0;

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto [_, reg] = app.registry.read();

        auto& style = reg.get<MeshStyle>(entity);
        if (ImGuiLTable::Checkbox("Wireframe", &style.wireframe))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Draw backfaces", &style.drawBackfaces))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Depth write", &style.writeDepth))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Two-pass alpha", &style.twoPassAlpha)) {
            if (style.twoPassAlpha)
                style.writeDepth = true;
            style.dirty(reg);
        }            

        ImGuiLTable::End();
    }
};



auto Demo_Mesh_Lighting = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [_, reg] = app.registry.write();

        // Create a new entity to host our mesh
        entity = reg.create();

        // Createa a 3D geometry
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Populate geom.vertices, geom.normals, geom.indices to create a solid
        // ellipsoidal shape with CCW triangle winding:
        const unsigned rings = 16;
        const unsigned sectors = 32;
        const double b = 1000000.0, c = 500000.0, a = 750000.0;
        for (unsigned r = 0; r <= rings; ++r)
        {
            double theta = glm::pi<double>() * double(r) / double(rings);
            double sin_theta = sin(theta);
            double cos_theta = cos(theta);
            for (unsigned s = 0; s <= sectors; ++s)
            {
                double phi = glm::two_pi<double>() * double(s) / double(sectors);
                double sin_phi = sin(phi);
                double cos_phi = cos(phi);
                glm::dvec3 n{ cos_phi * sin_theta, cos_theta, sin_phi * sin_theta };
                glm::dvec3 v{ a * n.x, b * n.y, c * n.z };
                geom.vertices.push_back(v);
                geom.normals.push_back(n);
            }
        }
        for (unsigned r = 0; r < rings; ++r)
        {
            for (unsigned s = 0; s < sectors; ++s)
            {
                unsigned i1 = r * (sectors + 1) + s;
                unsigned i2 = i1 + sectors + 1;
                geom.indices.push_back(i1);
                geom.indices.push_back(i1 + 1);
                geom.indices.push_back(i2);
                geom.indices.push_back(i1 + 1);
                geom.indices.push_back(i2 + 1);
                geom.indices.push_back(i2);
            }
        }

        auto& style = reg.emplace<MeshStyle>(entity);
        style.color = Color::Lime;
        style.lighting = true;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

        auto& xform = reg.emplace<Transform>(entity);
        xform.topocentric = true;
        xform.position = GeoPoint(SRS::WGS84, -25.0, 25.0, 250000.0);
    }


    if (ImGuiLTable::Begin("Mesh"))
    {
        auto [_, reg] = app.registry.read();

        auto& style = reg.get<MeshStyle>(entity);
        if (ImGuiLTable::Checkbox("Wireframe", &style.wireframe))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Lighting", &style.lighting))
            style.dirty(reg);

        if (ImGuiLTable::Checkbox("Draw backfaces", &style.drawBackfaces))
            style.dirty(reg);

        ImGuiLTable::End();
    }
};
