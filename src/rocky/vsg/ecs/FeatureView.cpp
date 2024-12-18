/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "FeatureView.h"
#include "Mesh.h"
#include "Transform.h"
#include "Visibility.h"
#include "../VSGContext.h"
#include <rocky/weemesh.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    // Transforms a range of points from geographic (long lat) to gnomonic coordinates
    // around a centroid with an optional scale.
    template<class T, class ITER>
    void geo_to_gnomonic(ITER begin, ITER end, const T& centroid, double scale = 1.0)
    {
        double lon0 = deg2rad(centroid.x);
        double lat0 = deg2rad(centroid.y);

        for (ITER p = begin; p != end; ++p)
        {
            double lon = deg2rad(p->x);
            double lat = deg2rad(p->y);
            double d = sin(lat0) * sin(lat) + cos(lat0) * cos(lat) * cos(lon - lon0);
            p->x = scale * (cos(lat) * sin(lon - lon0)) / d;
            p->y = scale * (cos(lat0) * sin(lat) - sin(lat0) * cos(lat) * cos(lon - lon0)) / d;
        }
    }

    // Transforms a range of points from gnomonic coordinates around a centroid with a
    // given scale to geographic (long lat) coordinates.
    template<class T, class ITER>
    void gnomonic_to_geo(ITER begin, ITER end, const T& centroid, double scale = 1.0)
    {
        double lon0 = deg2rad(centroid.x);
        double lat0 = deg2rad(centroid.y);

        for (ITER p = begin; p != end; ++p)
        {
            double x = p->x / scale, y = p->y / scale;
            double rho = sqrt(x * x + y * y);
            double c = atan(rho);

            double lat = asin(cos(c) * sin(lat0) + (y * sin(c) * cos(lat0) / rho));
            double lon = lon0 + atan((x * sin(c)) / (rho * cos(lat0) * cos(c) - y * sin(lat0) * sin(c)));

            p->x = rad2deg(lon);
            p->y = rad2deg(lat);
        }
    }

    template<class T>
    void tessellate_line_segment(const T& from, const T& to, const SRS& srs, const GeodeticInterpolation interp, float max_span, std::vector<T>& output, bool add_last_point)
    {
        //TODO: make it work for projected SRS?
        ROCKY_SOFT_ASSERT_AND_RETURN(srs.isGeodetic(), void());

        auto& ellipsoid = srs.ellipsoid();
        std::list<T> list{ from, to };
        auto iter = list.begin();
        for (;;)
        {
            auto save = iter;
            auto& p1 = *iter++;
            if (iter == list.end())
                break;
            auto& p2 = *iter;

            if (ellipsoid.geodesicGroundDistance(p1, p2) > max_span)
            {
                T midpoint;

                if (interp == GeodeticInterpolation::GreatCircle)
                {
                    midpoint = ellipsoid.geodesicInterpolate(p1, p2, 0.5);
                }
                else // GeodeticInterpolation::RhumbLine
                {
                    midpoint = (p1 + p2) * 0.5;
                }
                list.insert(iter, midpoint);
                iter = save;
            }
        }

        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            auto next = iter; next++;
            if (add_last_point || next != list.end())
                output.push_back(*iter);
        }
    }

    std::vector<glm::dvec3> tessellate_linestring(const std::vector<glm::dvec3>& input, const SRS& srs, GeodeticInterpolation interp, float max_span)
    {
        std::vector<glm::dvec3> output;
        if (input.size() > 0)
        {
            for (unsigned i = 1; i < input.size(); ++i)
            {
                tessellate_line_segment(input[i - 1], input[i], srs, interp, max_span, output, false);
            }
            output.push_back(input.back());
        }
        return output;
    }

    float get_max_segment_length(const std::vector<glm::dvec3>& input)
    {
        float m = 0.0f;
        for (unsigned i = 0; i < input.size() - 1; ++i)
        {
            m = std::max(m, (float)glm::length(input[i] - input[i + 1]));
        }
        return m;
    }

    void compile_feature_to_lines(const Feature& feature, const StyleSheet& styles, const SRS& geom_srs, Line& line)
    {
        float max_span = 100000.0f;

        if (styles.line.has_value())
            max_span = styles.line->resolution;

        float final_max_span = max_span;

        Geometry::const_iterator iter(feature.geometry);
        while (iter.hasMore())
        {
            auto& part = iter.next();

            // tessellate:
            auto tessellated = tessellate_linestring(part.points, feature.srs, feature.interpolation, max_span);

            // transform:
            auto feature_to_world = feature.srs.to(geom_srs);
            feature_to_world.transformRange(tessellated.begin(), tessellated.end());

            // Populate the line component based on the topology.
            if (line.topology == Line::Topology::Strip)
            {
                line.points.resize(line.points.size() + tessellated.size());

                std::transform(tessellated.begin(), tessellated.end(), line.points.begin(), 
                    [](const glm::dvec3& p) { return vsg::dvec3(p.x, p.y, p.z); });
            }
            else // Line::Topology::Segments
            {
                std::size_t num_points_in_segments = tessellated.size() * 2 - 2;
                auto ptr = line.points.size();
                line.points.resize(line.points.size() + num_points_in_segments);

                // convert from a strip to segments
                for (std::size_t i = 0; i < tessellated.size() - 1; ++i)
                {
                    line.points[ptr++] = vsg::dvec3(tessellated[i].x, tessellated[i].y, tessellated[i].z);
                    line.points[ptr++] = vsg::dvec3(tessellated[i + 1].x, tessellated[i + 1].y, tessellated[i + 1].z);
                }
            }

            final_max_span = std::max(final_max_span, get_max_segment_length(tessellated));
        }

        // max length:
        max_span = final_max_span;

        if (styles.line.has_value())
        {
            line.style = styles.line.value();
        }
    }

    void compile_polygon_feature_with_weemesh(const Feature& feature, const Geometry& geom, const StyleSheet& styles, const SRS& geom_srs, Mesh& mesh)
    {
        // scales our local gnomonic coordinates so they are the same order of magnitude as
        // weemesh's default epsilon values:
        const double gnomonic_scale = 1000.0;

        // Meshed triangles will be at a maximum this many degrees across in size,
        // to help follow the curvature of the earth.
        const double resolution_degrees = 0.25;

        // apply a fake Z for testing purposes before we attempt depth offsetting
        const double fake_z_offset = 0.0;


        // some conversions we will need:
        auto feature_geo = feature.srs.geodeticSRS();
        auto feature_to_geo = feature.srs.to(feature_geo);
        auto feature_to_world = feature.srs.to(geom_srs);

        // centroid for use with the gnomonic projection:
        glm::dvec3 centroid;
        feature.extent.getCentroid(centroid.x, centroid.y);
        feature_to_geo.transform(centroid, centroid);

        // transform to gnomonic. We are not using SRS/PROJ for the gnomonic projection
        // because it would require creating a new SRS for each and every feature (because
        // of the centroid) and that is way too slow.
        Geometry local_geom = geom; // working copy
        Box local_ex;
        Geometry::iterator iter(local_geom);
        while (iter.hasMore())
        {
            auto& part = iter.next();
            if (!part.points.empty())
            {
                feature_to_geo.transformRange(part.points.begin(), part.points.end());
                geo_to_gnomonic(part.points.begin(), part.points.end(), centroid, gnomonic_scale);
                local_ex.expandBy(part.points.begin(), part.points.end());
            }
        }

        // start with a weemesh covering the feature extent.
        weemesh::mesh_t m;
        int marker = 0;
        double xspan = gnomonic_scale * resolution_degrees * 3.14159 / 180.0;
        double yspan = gnomonic_scale * resolution_degrees * 3.14159 / 180.0;
        int cols = std::max(2, (int)(local_ex.width() / xspan));
        int rows = std::max(2, (int)(local_ex.height() / yspan));
        for (int row = 0; row < rows; ++row)
        {
            double v = (double)row / (double)(rows - 1);
            double y = local_ex.ymin + v * local_ex.height();

            for (int col = 0; col < cols; ++col)
            {
                double u = (double)col / (double)(cols - 1);
                double x = local_ex.xmin + u * local_ex.width();

                m.get_or_create_vertex_from_vec3(glm::dvec3{ x, y, 0.0 }, marker);
            }
        }

        for (int row = 0; row < rows - 1; ++row)
        {
            for (int col = 0; col < cols - 1; ++col)
            {
                int k = row * cols + col;
                m.add_triangle(k, k + 1, k + cols);
                m.add_triangle(k + 1, k + cols + 1, k + cols);
            }
        }

        // next, apply the segments of the polygon to slice the mesh into triangles.
        Geometry::const_iterator segment_iter(local_geom);
        while (segment_iter.hasMore())
        {
            auto& part = segment_iter.next();
            for (unsigned i = 0; i < part.points.size(); ++i)
            {
                unsigned j = (i == part.points.size() - 1) ? 0 : i + 1;
                m.insert(weemesh::segment_t{ part.points[i], part.points[j] }, marker);
            }
        }

        // next we need to remove all the exterior triangles.
        std::unordered_set<weemesh::triangle_t*> insiders;
        std::unordered_set<weemesh::triangle_t*> outsiders;
        Geometry::const_iterator remove_iter(local_geom, false);
        while (remove_iter.hasMore())
        {
            auto& part = remove_iter.next();

            for (auto& tri_iter : m.triangles)
            {
                weemesh::triangle_t& tri = tri_iter.second;
                auto c = (tri.p0 + tri.p1 + tri.p2) * (1.0 / 3.0); // centroid
                bool inside = part.contains(c.x, c.y);
                if (inside)
                    insiders.insert(&tri);
                else
                    outsiders.insert(&tri);
            }
        }
        for (auto tri : outsiders)
        {
            if (insiders.count(tri) == 0)
            {
                m.remove_triangle(*tri);
            }
        }

        for (auto& v : m.verts)
            v.z += fake_z_offset;

        // Back to geographic:
        gnomonic_to_geo(m.verts.begin(), m.verts.end(), centroid, gnomonic_scale);

        // And into the final projection:
        feature_to_world.transformRange(m.verts.begin(), m.verts.end());

        auto color =
            styles.mesh_function ? styles.mesh_function(feature).color :
            styles.mesh.has_value() ? styles.mesh->color :
            vsg::vec4(1, 1, 1, 1);

        auto depth_offset =
            styles.mesh_function ? styles.mesh_function(feature).depth_offset :
            styles.mesh.has_value() ? styles.mesh->depth_offset :
            0.0f;

        Triangle temp = {
            {}, // we'll fill in the verts below
            {color, color, color},
            {}, // uvs - don't need them
            {depth_offset, depth_offset, depth_offset} }; // depth offset values

        for (auto& tri : m.triangles)
        {
            temp.verts[0] = m.verts[tri.second.i0];
            temp.verts[1] = m.verts[tri.second.i1];
            temp.verts[2] = m.verts[tri.second.i2];
            mesh.triangles.emplace_back(temp);
        }
    }
}


FeatureView::FeatureView()
{
    //nop
}

FeatureView::FeatureView(const Feature& f)
{
    features.emplace_back(f);
}

FeatureView::FeatureView(Feature&& f) noexcept
{
    features.emplace_back(f);
}

void
FeatureView::clear(entt::registry& registry)
{
    for (auto entity : line_entities)
    {
        if (entity != entt::null)
        {
            registry.remove<Line>(entity);
        }
    }
    line_entities.clear();

    for (auto entity : mesh_entities)
    {
        if (entity != entt::null)
        {
            registry.remove<Mesh>(entity);
        }
    }
    mesh_entities.clear();
}

void
FeatureView::generate(entt::registry& registry, const SRS& geom_srs, VSGContext& runtime, bool keep_features)
{
    if (entity == entt::null)
    {
        entity = registry.create();
        registry.emplace<Visibility>(entity);
    }

    auto& host_visibility = registry.get<Visibility>(entity);

    for (auto& feature : features)
    {
        if (feature.geometry.type == Geometry::Type::LineString ||
            feature.geometry.type == Geometry::Type::MultiLineString)
        {
            if (line_entities.empty())
                line_entities.emplace_back(registry.create());

            auto& entity = line_entities.front();
            auto& geom = registry.get_or_emplace<Line>(entity);
            geom.topology = Line::Topology::Segments;

            registry.get<Visibility>(entity).parent = &host_visibility;

            compile_feature_to_lines(feature, styles, geom_srs, geom);
        }
        else if (feature.geometry.type == Geometry::Type::Polygon)
        {
            auto entity = mesh_entities.emplace_back(registry.create());
            auto& geom = registry.get_or_emplace<Mesh>(entity);
            registry.get<Visibility>(entity).parent = &host_visibility;

            compile_polygon_feature_with_weemesh(feature, feature.geometry, styles, geom_srs, geom);
        }
        else if (feature.geometry.type == Geometry::Type::MultiPolygon)
        {
            auto entity = mesh_entities.emplace_back(registry.create());
            auto& geom = registry.get_or_emplace<Mesh>(entity);
            registry.get<Visibility>(entity).parent = &host_visibility;

            for (auto& part : feature.geometry.parts)
            {
                compile_polygon_feature_with_weemesh(feature, part, styles, geom_srs, geom);
            }
        }
        else
        {
            Log()->warn("FeatureView no support for " + Geometry::typeToString(feature.geometry.type));
        }
    }

    if (!keep_features)
    {
        features.clear();
    }
}

void
FeatureView::dirtyStyles(entt::registry& registry)
{
    if (line_entities.empty() && mesh_entities.empty())
        return;

    if (styles.line.has_value())
    {
        for (auto entity : line_entities)
        {
            if (auto* line = registry.try_get<Line>(entity))
            {
                line->style = styles.line.value();
                line->dirty();
            }
        }
    }

    if (styles.mesh.has_value())
    {
        for (auto entity : mesh_entities)
        {
            if (auto* mesh = registry.try_get<Mesh>(entity))
            {
                mesh->style = styles.mesh.value();
                mesh->dirty();
            }
        }
    }
}