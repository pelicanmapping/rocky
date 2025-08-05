/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "FeatureView.h"
#include <rocky/ElevationSampler.h>
#include <rocky/weemesh.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

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
    void tessellate_line_segment(const T& from, const T& to, const SRS& input_srs, const GeodeticInterpolation interp, float max_span, std::vector<T>& output, bool add_last_point)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(input_srs.isGeodetic(), void());

        auto& ellipsoid = input_srs.ellipsoid();
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

    std::vector<glm::dvec3> tessellate_linestring(const std::vector<glm::dvec3>& input, const SRS& input_srs, GeodeticInterpolation interp, float max_span)
    {
        std::vector<glm::dvec3> output;

        if (input.size() > 0)
        {
            // only geodetic coordinates get tessellated for now:
            if (input_srs.isGeodetic())
            {
                for (unsigned i = 1; i < input.size(); ++i)
                {
                    tessellate_line_segment(input[i - 1], input[i], input_srs, interp, max_span, output, false);
                }
                output.push_back(input.back());
            }
            else
            {
                output = input;
            }
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

    void compile_feature_to_lines(const Feature& feature, const StyleSheet& styles, const GeoPoint& origin,
        ElevationSession& clamper, const SRS& output_srs, Line& line)
    {
        float max_span = styles.line.resolution;

        float final_max_span = max_span;

        feature.geometry.eachPart([&](const Geometry& part)
            {
                if (part.points.size() < 2)
                    return;

                // tessellate:
                auto tessellated = tessellate_linestring(part.points, feature.srs, feature.interpolation, max_span);

                // clamp:
                if (clamper)
                {
                    clamper.clampRange(tessellated.begin(), tessellated.end());
                }

                // transform:
                auto feature_to_world = feature.srs.to(output_srs);
                feature_to_world.transformRange(tessellated.begin(), tessellated.end());

                // localize:
                if (origin.valid())
                {
                    auto ref_out = origin.transform(output_srs);
                    for (auto& p : tessellated)
                    {
                        p -= glm::dvec3(ref_out.x, ref_out.y, ref_out.z);
                    }
                }

                // Populate the line component based on the topology.
                if (line.topology == Line::Topology::Strip)
                {
                    // CHECK THIS
                    line.points.reserve(line.points.size() + tessellated.size());
                    line.points.insert(line.points.end(), tessellated.begin(), tessellated.end());
                }

                else // Line::Topology::Segments
                {
                    std::size_t num_points_in_segments = tessellated.size() * 2 - 2;
                    auto ptr = line.points.size();
                    line.points.resize(line.points.size() + num_points_in_segments);

                    // convert from a strip to segments
                    for (std::size_t i = 0; i < tessellated.size() - 1; ++i)
                    {
                        line.points[ptr++] = glm::dvec3(tessellated[i].x, tessellated[i].y, tessellated[i].z);
                        line.points[ptr++] = glm::dvec3(tessellated[i + 1].x, tessellated[i + 1].y, tessellated[i + 1].z);
                    }
                }

                final_max_span = std::max(final_max_span, get_max_segment_length(tessellated));
            });

        // max length:
        max_span = final_max_span;

        line.style = styles.line;
    }

    void compile_polygon_feature_with_weemesh(const Feature& feature, const StyleSheet& styles, 
        const GeoPoint& origin, ElevationSession& clamper, const SRS& output_srs, Mesh& mesh)
    {
        // scales our local gnomonic coordinates so they are the same order of magnitude as
        // weemesh's default epsilon values:
        const double gnomonic_scale = 1e6; // 1.0; // 1000.0;

        // Meshed triangles will be at a maximum this many degrees across in size,
        // to help follow the curvature of the earth.
        const double resolution_degrees = 0.25;

        // apply a fake Z for testing purposes before we attempt depth offsetting
        const double fake_z_offset = 0.0;


        // some conversions we will need:
        auto feature_geo = feature.srs.geodeticSRS();
        auto feature_to_geo = feature.srs.to(feature_geo);
        auto geo_to_world = feature_geo.to(output_srs);

        // centroid for use with the gnomonic projection:
        glm::dvec3 centroid;
        feature.extent.getCentroid(centroid.x, centroid.y);
        feature_to_geo.transform(centroid, centroid);

        // transform to gnomonic. We are not using SRS/PROJ for the gnomonic projection
        // because it would require creating a new SRS for each and every feature (because
        // of the centroid) and that is way too slow.
        Geometry local_geom = feature.geometry; // working copy
        Box local_ex;

        // transform the geometry to gnomonic coordinates, and establish the extent.
        local_geom.eachPart([&](Geometry& part)
            {
                feature_to_geo.transformRange(part.points.begin(), part.points.end());
                geo_to_gnomonic(part.points.begin(), part.points.end(), centroid, gnomonic_scale);
                local_ex.expandBy(part.points.begin(), part.points.end());
            });

        // start with a tessellated weemesh covering the feature extent.
        // The amount of tessellation is determined by the resolution_degrees to account
        // for the planet's curvature.
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
        local_geom.eachPart([&](const Geometry& part)
            {
                for (unsigned i = 0; i < part.points.size(); ++i)
                {
                    unsigned j = (i == part.points.size() - 1) ? 0 : i + 1;
                    m.insert(weemesh::segment_t{ part.points[i], part.points[j] }, marker | m._has_elevation_marker);
                }
            });

        // next we need to remove all the exterior triangles.
        std::unordered_set<weemesh::triangle_t*> insiders;
        std::unordered_set<weemesh::triangle_t*> outsiders;

        Geometry::const_iterator(local_geom, false).eachPart([&](const Geometry& part)
            {
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
            });

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

        // Clamp any points that are not marked as having elevation.
        if (clamper)
        {
            clamper.srs = feature_geo;
            clamper.clampRange(m.verts.begin(), m.verts.end());
        }

        // And into the final projection:
        geo_to_world.transformRange(m.verts.begin(), m.verts.end());

        // localize:
        if (origin.valid())
        {
            auto ref_out = origin.transform(output_srs);
            for (auto& p : m.verts)
            {
                p = p - weemesh::vert_t(ref_out.x, ref_out.y, ref_out.z);
            }
        }

        auto color =
            styles.mesh_function ? styles.mesh_function(feature).color :
            styles.mesh.color;

        auto depth_offset =
            styles.mesh_function ? styles.mesh_function(feature).depth_offset :
            styles.mesh.depth_offset;

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


FeatureView::Primitives
FeatureView::generate(const SRS& output_srs)
{
    Primitives output;
    output.line.topology = Line::Topology::Segments;

    for (auto& feature : features)
    {
        clamper.srs = feature.srs;

        // If the output is geocentric, do all our processing in geodetic coordinates.
        if (output_srs.isGeocentric())
        {
            feature.transformInPlace(output_srs.geodeticSRS());
            clamper.srs = output_srs.geodeticSRS();
        }

        if (feature.geometry.type == Geometry::Type::LineString ||
            feature.geometry.type == Geometry::Type::MultiLineString)
        {
            compile_feature_to_lines(feature, styles, origin, clamper, output_srs, output.line);
        }

        else if (feature.geometry.type == Geometry::Type::Polygon ||
            feature.geometry.type == Geometry::Type::MultiPolygon)
        {
            compile_polygon_feature_with_weemesh(feature, styles, origin, clamper, output_srs, output.mesh);
        }

        else
        {
            Log()->warn("FeatureView no support for " + Geometry::typeToString(feature.geometry.type));
        }
    }

    return output;
}

void
FeatureView::generate(FeatureView::PrimitivesRef& output, const SRS& output_srs)
{
    for (auto& feature : features)
    {
        // If the output is geocentric, do all our processing in geodetic coordinates.
        if (output_srs.isGeocentric())
        {
            feature.transformInPlace(output_srs.geodeticSRS());
        }

        if (feature.geometry.type == Geometry::Type::LineString ||
            feature.geometry.type == Geometry::Type::MultiLineString)
        {
            if (output.line)
            {
                compile_feature_to_lines(feature, styles, origin, clamper, output_srs, *output.line);
            }
        }

        else if (feature.geometry.type == Geometry::Type::Polygon ||
            feature.geometry.type == Geometry::Type::MultiPolygon)
        {
            if (output.mesh)
            {
                compile_polygon_feature_with_weemesh(feature, styles, origin, clamper, output_srs, *output.mesh);
            }
        }

        else
        {
            Log()->warn("FeatureView no support for " + Geometry::typeToString(feature.geometry.type));
        }
    }
}
