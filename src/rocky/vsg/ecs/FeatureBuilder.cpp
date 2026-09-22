/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "FeatureBuilder.h"
#include <rocky/ElevationSampler.h>
#include <rocky/weemesh.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Normalizes a longitude to [-180, 180), treating both antimeridian signs
    //! as the same seam value.
    double normalize_longitude(double x)
    {
        constexpr double EPS = 1e-8;
        if (fabs(x - (-180.0)) < EPS || fabs(x - 180.0) < EPS)
            return -180.0;

        while (x < -180.0)
            x += 360.0;
        while (x >= 180.0)
            x -= 360.0;

        return x;
    }

    //! Computes the midpoint of a geodetic segment using longitude/latitude
    //! interpolation in an unwrapped longitude frame.
    template<class T>
    T rhumb_midpoint(const T& p1, const T& p2)
    {
        T p2_unwrapped = p2;

        while (p2_unwrapped.x - p1.x > 180.0)
            p2_unwrapped.x -= 360.0;
        while (p2_unwrapped.x - p1.x < -180.0)
            p2_unwrapped.x += 360.0;

        T midpoint = (p1 + p2_unwrapped) * 0.5;
        midpoint.x = normalize_longitude(midpoint.x);
        return midpoint;
    }

    //! Transforms a range of geodetic longitude/latitude points to local gnomonic
    //! coordinates around a feature-specific centroid.
    template<class T, class ITER>
    void geo_to_gnomonic(ITER begin, ITER end, const T& centroid, double scale = 1.0)
    {
        double lon0 = glm::radians(centroid.x);
        double lat0 = glm::radians(centroid.y);

        for (ITER p = begin; p != end; ++p)
        {
            double lon = glm::radians(p->x);
            double lat = glm::radians(p->y);
            double d = sin(lat0) * sin(lat) + cos(lat0) * cos(lat) * cos(lon - lon0);
            p->x = scale * (cos(lat) * sin(lon - lon0)) / d;
            p->y = scale * (cos(lat0) * sin(lat) - sin(lat0) * cos(lat) * cos(lon - lon0)) / d;
        }
    }

    //! Transforms a range of local gnomonic points back to geodetic
    //! longitude/latitude coordinates around the same centroid.
    template<class T, class ITER>
    void gnomonic_to_geo(ITER begin, ITER end, const T& centroid, double scale = 1.0)
    {
        double lon0 = glm::radians(centroid.x);
        double lat0 = glm::radians(centroid.y);

        for (ITER p = begin; p != end; ++p)
        {
            double x = p->x / scale, y = p->y / scale;
            double rho = sqrt(x * x + y * y);
            if (rho == 0.0)
            {
                p->x = centroid.x;
                p->y = centroid.y;
                continue;
            }

            double c = atan(rho);

            double lat = asin(cos(c) * sin(lat0) + (y * sin(c) * cos(lat0) / rho));
            double lon = lon0 + atan2(
                x * sin(c),
                rho * cos(lat0) * cos(c) - y * sin(lat0) * sin(c));

            p->x = normalize_longitude(glm::degrees(lon));
            p->y = glm::degrees(lat);
        }
    }

    //! Recursively subdivides one geodetic or geocentric segment until it meets
    //! the maximum span, appending points to the output vector.
    template<class T>
    void tessellate_line_segment(const T& from, const T& to, const SRS& input_srs, const GeodeticInterpolation interp, float max_span, std::vector<T>& output, bool add_last_point)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(input_srs.isGeodetic() || input_srs.isGeocentric(), void());

        auto& ellipsoid = input_srs.ellipsoid();
        std::list<T> list{ from, to };
        auto iter = list.begin();

        if (input_srs.isGeodetic())
        {
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
                        midpoint = rhumb_midpoint(p1, p2);
                    }
                    list.insert(iter, midpoint);
                    iter = save;
                }
            }
        }
        else if (input_srs.isGeocentric())
        {
            auto max_span_squared = max_span * max_span;

            for (;;)
            {
                auto save = iter;
                auto& p1 = *iter++;
                if (iter == list.end())
                    break;
                auto& p2 = *iter;

                if (glm::dot(p2 - p1, p2 - p1) > max_span_squared) // dot is length squared
                {
                    T midpoint = ellipsoid.geocentricInterpolate(p1, p2, 0.5);
                    list.insert(iter, midpoint);
                    iter = save;
                }
            }
        }

        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            auto next = iter; next++;
            if (add_last_point || next != list.end())
                output.push_back(*iter);
        }
    }

    //! Tessellates a polyline according to its SRS and geodetic interpolation mode.
    std::vector<glm::dvec3> tessellate_linestring(const std::vector<glm::dvec3>& input, const SRS& input_srs, GeodeticInterpolation interp, float max_span)
    {
        std::vector<glm::dvec3> output;

        if (input.size() > 0)
        {
            // only geodetic coordinates get tessellated for now:
            if (input_srs.isGeodetic() || input_srs.isGeocentric())
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

    //! Splits a geodetic line string into separate parts wherever it crosses the
    //! antimeridian, inserting seam vertices on both sides of the longitude wrap.
    std::vector<std::vector<glm::dvec3>> split_linestring_at_antimeridian(const std::vector<glm::dvec3>& input)
    {
        std::vector<std::vector<glm::dvec3>> output;

        if (input.empty())
            return output;

        output.emplace_back();
        output.back().emplace_back(input.front());

        for (std::size_t i = 1; i < input.size(); ++i)
        {
            const auto& a = input[i - 1];
            const auto& b = input[i];
            double delta = b.x - a.x;

            if (fabs(delta) > 180.0)
            {
                glm::dvec3 b_unwrapped = b;
                double first_edge_lon;
                double second_edge_lon;

                if (delta < -180.0)
                {
                    b_unwrapped.x += 360.0;
                    first_edge_lon = 180.0;
                    second_edge_lon = -180.0;
                }
                else
                {
                    b_unwrapped.x -= 360.0;
                    first_edge_lon = -180.0;
                    second_edge_lon = 180.0;
                }

                double denom = b_unwrapped.x - a.x;
                double t = fabs(denom) > 1e-12 ? (first_edge_lon - a.x) / denom : 0.0;
                t = std::max(0.0, std::min(1.0, t));

                glm::dvec3 first_edge = a + (b_unwrapped - a) * t;
                first_edge.x = first_edge_lon;

                glm::dvec3 second_edge = first_edge;
                second_edge.x = second_edge_lon;

                output.back().emplace_back(first_edge);
                output.emplace_back();
                output.back().emplace_back(second_edge);
            }

            output.back().emplace_back(b);
        }

        return output;
    }

    //! Tessellates a closed polygon ring while preserving the ring as an implicit
    //! loop instead of duplicating the first vertex at the end.
    std::vector<glm::dvec3> tessellate_polygon_ring(
        const std::vector<glm::dvec3>& input,
        const SRS& input_srs,
        GeodeticInterpolation interp,
        float max_span)
    {
        std::vector<glm::dvec3> output;

        if (input.size() < 2)
            return input;

        bool closed = input.front() == input.back();
        std::size_t ring_size = closed ? input.size() - 1 : input.size();

        if (ring_size < 2)
            return input;

        for (std::size_t i = 0; i < ring_size; ++i)
        {
            std::size_t j = i == ring_size - 1 ? 0 : i + 1;
            tessellate_line_segment(input[i], input[j], input_srs, interp, max_span, output, false);
        }

        return output;
    }

    //! Tessellates every ring in a polygon or multipolygon. For polygons, points
    //! remain the outer ring and parts remain simple hole rings.
    void tessellate_polygon_edges(Geometry& geometry, const SRS& input_srs, GeodeticInterpolation interp, float max_span)
    {
        if (!input_srs.isGeodetic() && !input_srs.isGeocentric())
            return;

        geometry.eachPart([&](Geometry& part)
            {
                if (part.points.size() >= 2)
                    part.points = tessellate_polygon_ring(part.points, input_srs, interp, max_span);
            });
    }

    //! Returns the longest straight segment length in the input coordinate system.
    float get_max_segment_length(const std::vector<glm::dvec3>& input)
    {
        float m = 0.0f;
        for (unsigned i = 0; i < input.size() - 1; ++i)
        {
            m = std::max(m, (float)glm::length(input[i] - input[i + 1]));
        }
        return m;
    }

    //! Computes great-circle angular distance in degrees between two geodetic points.
    double angular_distance_degrees(const glm::dvec3& a, const glm::dvec3& b)
    {
        double lon1 = glm::radians(a.x);
        double lat1 = glm::radians(a.y);
        double lon2 = glm::radians(b.x);
        double lat2 = glm::radians(b.y);
        double dlon = lon2 - lon1;
        double dlat = lat2 - lat1;
        double sin_dlat = sin(dlat * 0.5);
        double sin_dlon = sin(dlon * 0.5);
        double h = sin_dlat * sin_dlat + cos(lat1) * cos(lat2) * sin_dlon * sin_dlon;
        return glm::degrees(2.0 * asin(std::min(1.0, sqrt(h))));
    }

    //! Estimates a geodetic center by averaging latitude and using a circular mean
    //! for longitude so antimeridian-spanning geometries stay centered correctly.
    bool get_geometry_geodetic_center(const Geometry& geometry, glm::dvec3& out_center)
    {
        double lon_sin_sum = 0.0;
        double lon_cos_sum = 0.0;
        double lat_sum = 0.0;
        std::size_t point_count = 0;

        geometry.eachPart([&](const Geometry& part)
            {
                for (const auto& p : part.points)
                {
                    double lon = glm::radians(p.x);
                    lon_sin_sum += sin(lon);
                    lon_cos_sum += cos(lon);
                    lat_sum += p.y;
                    ++point_count;
                }
            });

        if (point_count == 0)
            return false;

        out_center.x = glm::degrees(atan2(lon_sin_sum, lon_cos_sum));
        out_center.y = lat_sum / (double)point_count;
        out_center.z = 0.0;
        return true;
    }

    //! Measures the largest angular distance from the supplied center to any point
    //! in a geometry, including polygon holes and multipolygon children.
    double max_angular_radius_degrees(const Geometry& geometry, const glm::dvec3& center)
    {
        double max_radius = 0.0;

        geometry.eachPart([&](const Geometry& part)
            {
                for (const auto& p : part.points)
                {
                    max_radius = std::max(max_radius, angular_distance_degrees(center, p));
                }
            });

        return max_radius;
    }

    //! Unwraps all longitudes into a continuous range around center_lon to make
    //! planar bounds and splitting meaningful near the antimeridian.
    void unwrap_geometry_longitudes(Geometry& geometry, double center_lon)
    {
        geometry.eachPart([&](Geometry& part)
            {
                for (auto& p : part.points)
                {
                    while (p.x - center_lon > 180.0)
                        p.x -= 360.0;
                    while (p.x - center_lon < -180.0)
                        p.x += 360.0;
                }
            });
    }

    //! Computes bounds across all populated simple parts of a geometry.
    Box get_geometry_bounds(const Geometry& geometry)
    {
        Box bounds;

        geometry.eachPart([&](const Geometry& part)
            {
                bounds.expandBy(part.points.begin(), part.points.end());
            });

        return bounds;
    }

    //! Linearly interpolates a segment to the point where it crosses a chosen axis.
    glm::dvec3 interpolate_to_axis(const glm::dvec3& a, const glm::dvec3& b, int axis, double value)
    {
        double av = axis == 0 ? a.x : a.y;
        double bv = axis == 0 ? b.x : b.y;
        double t = (value - av) / (bv - av);
        return a + (b - a) * t;
    }

    //! Clips one ring to an axis-aligned half-plane using Sutherland-Hodgman clipping.
    std::vector<glm::dvec3> clip_ring_to_halfplane(
        const std::vector<glm::dvec3>& input, int axis, double value, bool keep_less)
    {
        std::vector<glm::dvec3> output;

        if (input.empty())
            return output;

        auto inside = [&](const glm::dvec3& p)
            {
                double v = axis == 0 ? p.x : p.y;
                return keep_less ? v <= value : v >= value;
            };

        auto previous = input.back();
        bool previous_inside = inside(previous);

        for (const auto& current : input)
        {
            bool current_inside = inside(current);

            if (current_inside != previous_inside)
            {
                output.emplace_back(interpolate_to_axis(previous, current, axis, value));
            }

            if (current_inside)
            {
                output.emplace_back(current);
            }

            previous = current;
            previous_inside = current_inside;
        }

        return output;
    }

    //! Clips one polygon to an axis-aligned half-plane. The output preserves the
    //! Geometry contract: points is the outer ring and parts contains simple holes.
    Geometry clip_polygon_to_halfplane(const Geometry& polygon, int axis, double value, bool keep_less)
    {
        Geometry output(Geometry::Type::Polygon);
        output.points = clip_ring_to_halfplane(polygon.points, axis, value, keep_less);

        if (output.points.size() < 3)
            return Geometry(Geometry::Type::Polygon);

        for (const auto& hole : polygon.parts)
        {
            auto clipped_hole = hole;
            clipped_hole.points = clip_ring_to_halfplane(hole.points, axis, value, keep_less);
            clipped_hole.parts.clear();
            if (clipped_hole.points.size() >= 3)
                output.parts.emplace_back(std::move(clipped_hole));
        }

        return output;
    }

    //! Clips a polygon or multipolygon to an axis-aligned half-plane while preserving
    //! simple vs. multi geometry layout.
    Geometry clip_geometry_to_halfplane(const Geometry& geometry, int axis, double value, bool keep_less)
    {
        if (geometry.type == Geometry::Type::Polygon)
        {
            return clip_polygon_to_halfplane(geometry, axis, value, keep_less);
        }

        if (geometry.type == Geometry::Type::MultiPolygon)
        {
            Geometry output(Geometry::Type::MultiPolygon);

            Geometry::const_iterator(geometry, false).eachPart([&](const Geometry& part)
                {
                    auto clipped_part = clip_polygon_to_halfplane(part, axis, value, keep_less);
                    if (clipped_part.points.size() >= 3)
                        output.parts.emplace_back(std::move(clipped_part));
                });

            return output;
        }

        return Geometry(geometry.type);
    }

    //! Normalizes all longitude values in a geometry after processing in an
    //! unwrapped longitude frame.
    void normalize_geometry_longitudes(Geometry& geometry)
    {
        geometry.eachPart([&](Geometry& part)
            {
                for (auto& p : part.points)
                    p.x = normalize_longitude(p.x);
            });
    }

    //! Converts line features into renderable ECS line coordinates, including
    //! tessellation, antimeridian splitting when needed, clamping, projection, and localization.
    void compile_feature_to_lines(const Feature& feature, const LineStyle& style, const GeoPoint& origin,
        ElevationSession& clamper, const SRS& output_srs, LineGeometry& lineGeom)
    {
        float max_span = style.resolution;

        float final_max_span = max_span;

        feature.geometry.eachPart([&](const Geometry& part)
            {
                if (part.points.size() < 2)
                    return;

                // tessellate:
                auto tessellated = tessellate_linestring(part.points, feature.srs, feature.interpolation, max_span);

                auto line_parts = feature.srs.isGeodetic() && !output_srs.isGeocentric() ?
                    split_linestring_at_antimeridian(tessellated) :
                    std::vector<std::vector<glm::dvec3>>{ std::move(tessellated) };

                for (auto& line_part : line_parts)
                {
                    if (line_part.size() < 2)
                        continue;

                    // clamp:
                    if (clamper)
                    {
                        clamper.clampRange(line_part.begin(), line_part.end());
                    }

                    auto feature_to_world = feature.srs.to(output_srs);

                    // transform:
                    feature_to_world.transformArray(line_part.data(), line_part.size());

                    // localize:
                    if (origin.valid())
                    {
                        auto ref_out = origin.transform(output_srs);
                        for (auto& p : line_part)
                        {
                            p -= glm::dvec3(ref_out.x, ref_out.y, ref_out.z);
                        }
                    }

                    // Populate the line component based on the topology.
                    if (lineGeom.topology == LineTopology::Strip)
                    {
                        // CHECK THIS
                        lineGeom.points.reserve(lineGeom.points.size() + line_part.size());
                        lineGeom.points.insert(lineGeom.points.end(), line_part.begin(), line_part.end());
                    }

                    else // Line::Topology::Segments
                    {
                        std::size_t num_points_in_segments = line_part.size() * 2 - 2;
                        auto ptr = lineGeom.points.size();
                        lineGeom.points.reserve(lineGeom.points.size() + num_points_in_segments);

                        // convert from a strip to segments
                        for (std::size_t i = 0; i < line_part.size() - 1; ++i)
                        {
                            lineGeom.points.emplace_back(line_part[i]);
                            lineGeom.points.emplace_back(line_part[i + 1]);
                        }
                    }

                    final_max_span = std::max(final_max_span, get_max_segment_length(line_part));
                }
            });

        // max length:
        max_span = final_max_span;
    }

    //! Tessellates one polygon feature in a local gnomonic frame with weemesh and
    //! emits the resulting triangles into MeshGeometry in the requested output SRS.
    void compile_polygon_feature_with_weemesh(const Feature& feature, const MeshStyle& style,
        const GeoPoint& origin, ElevationSession& clamper, const SRS& output_srs, MeshGeometry& meshGeom)
    {
        // scales our local gnomonic coordinates so they are the same order of magnitude as
        // weemesh's default epsilon values:
        const double gnomonic_scale = 1e6;

        // apply a fake Z for testing purposes before we attempt depth offsetting
        const double fake_z_offset = 0.0;


        // some conversions we will need:
        auto feature_geo = feature.srs.geodeticSRS();
        auto feature_to_geo = feature.srs.to(feature_geo);
        auto geo_to_world = feature_geo.to(output_srs);

        // Meshed triangles will be at a maximum this many degrees across in size,
        // to help follow the curvature of the earth. Input is in meters and we
        // convert to angular degrees using the output geodetic ellipsoid.
        double meters_per_degree = 111319.49079327357; // WGS84-equatorial fallback
        if (feature_geo.valid())
        {
            const double semiMajor = feature_geo.ellipsoid().semiMajorAxis();
            if (semiMajor > 0.0)
                meters_per_degree = (3.14159265358979323846 * semiMajor) / 180.0;
        }

        // The default is a curvature limit, not a request to subdivide every
        // feature into a fixed grid. Deriving this as feature-size / 16 makes
        // tiny, numerous polygons (building footprints in particular) produce
        // hundreds of seed vertices apiece for no visible benefit.
        double resolution_meters = style.resolution > 0.0f ?
            static_cast<double>(style.resolution) : 27830.0; // ~= 0.25 degrees at the equator
        resolution_meters = std::max(1.0, resolution_meters);
        const double resolution_degrees = std::max(1e-9, resolution_meters / meters_per_degree);

        // centroid for use with the gnomonic projection:
        glm::dvec3 centroid(0.0);
        bool have_centroid = false;

        if (feature.srs.isGeodetic())
        {
            double lon_sin_sum = 0.0;
            double lon_cos_sum = 0.0;
            double lat_sum = 0.0;
            std::size_t point_count = 0;

            feature.geometry.eachPart([&](const Geometry& part)
                {
                    for (auto p : part.points)
                    {
                        feature_to_geo.transform(p, p);
                        double lon = glm::radians(p.x);
                        lon_sin_sum += sin(lon);
                        lon_cos_sum += cos(lon);
                        lat_sum += p.y;
                        ++point_count;
                    }
                });

            if (point_count > 0)
            {
                centroid.x = glm::degrees(atan2(lon_sin_sum, lon_cos_sum));
                centroid.y = lat_sum / (double)point_count;
                have_centroid = true;
            }
        }
        else
        {
            Box feature_bounds;

            feature.geometry.eachPart([&](const Geometry& part)
                {
                    feature_bounds.expandBy(part.points.begin(), part.points.end());
                });

            if (feature_bounds.valid())
            {
                centroid = feature_bounds.center();
                have_centroid = feature_to_geo.transform(centroid, centroid);
            }
        }

        if (!have_centroid && feature.extent.getCentroid(centroid.x, centroid.y))
        {
            feature_to_geo.transform(centroid, centroid);
        }

        // transform to gnomonic. We are not using SRS/PROJ for the gnomonic projection
        // because it would require creating a new SRS for each and every feature (because
        // of the centroid) and that is way too slow.
        Geometry local_geom = feature.geometry; // working copy
        Box local_ex;

        // transform the geometry to gnomonic coordinates, and establish the extent.
        double source_z_sum = 0.0;
        std::size_t source_z_count = 0u;
        local_geom.eachPart([&](Geometry& part)
            {
                feature_to_geo.transformArray(part.points.data(), part.points.size());
                geo_to_gnomonic(part.points.begin(), part.points.end(), centroid, gnomonic_scale);
                local_ex.expandBy(part.points.begin(), part.points.end());
                for (const auto& point : part.points)
                {
                    source_z_sum += point.z;
                    ++source_z_count;
                }
            });

        // PolygonGeometry may already contain sampled elevations even though
        // this deferred triangulation has no ElevationSession. Seed new mesh
        // vertices near the source surface instead of silently putting them at
        // ellipsoid height. A live clamper, when present, still replaces these.
        const double seed_z = source_z_count > 0u ?
            source_z_sum / static_cast<double>(source_z_count) : 0.0;

        // start with a tessellated weemesh covering the feature extent.
        // The amount of tessellation is determined by the resolution_degrees to account
        // for the planet's curvature.
        weemesh::mesh_t m;

        int marker = 0;

        double xspan = gnomonic_scale * resolution_degrees * 3.14159 / 180.0;
        double yspan = gnomonic_scale * resolution_degrees * 3.14159 / 180.0;
        
        int cols = std::max(2, (int)(local_ex.width() / xspan));
        int rows = std::max(2, (int)(local_ex.height() / yspan));

        // Bound the initial grid's cost; boundary cuts can add more vertices.
        constexpr int max_grid_vertices = 60000;
        while ((long long)cols * (long long)rows > max_grid_vertices)
        {
            if (cols >= rows && cols > 2)
                cols = (cols + 1) / 2;
            else if (rows > 2)
                rows = (rows + 1) / 2;
            else
                break;
        }

        std::vector<int> grid_indices;
        grid_indices.reserve((std::size_t)cols * (std::size_t)rows);

        for (int row = 0; row < rows; ++row)
        {
            double v = (double)row / (double)(rows - 1);
            double y = local_ex.ymin + v * local_ex.height();

            for (int col = 0; col < cols; ++col)
            {
                double u = (double)col / (double)(cols - 1);
                double x = local_ex.xmin + u * local_ex.width();

                grid_indices.emplace_back(
                    m.get_or_create_vertex_from_vec3(glm::dvec3{ x, y, seed_z }, marker));
            }
        }

        for (int row = 0; row < rows - 1; ++row)
        {
            for (int col = 0; col < cols - 1; ++col)
            {
                int k = row * cols + col;
                int i00 = grid_indices[k];
                int i10 = grid_indices[k + 1];
                int i01 = grid_indices[k + cols];
                int i11 = grid_indices[k + cols + 1];

                if (i00 >= 0 && i10 >= 0 && i01 >= 0)
                    m.add_triangle(i00, i10, i01);

                if (i10 >= 0 && i11 >= 0 && i01 >= 0)
                    m.add_triangle(i10, i11, i01);
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

            //TODO: implement this IF we need it in the future.
            //clamper.clampRange(m.verts.begin(), m.verts.end(),
            //    [&](const weemesh::vert_t& p) {
            //        return (m.get_marker(p) & m._has_elevation_marker) == 0;
            //    });
        }

        // And into the final projection:
        geo_to_world.transformArray(m.verts.data(), m.verts.size());

        // localize:
        if (origin.valid())
        {
            auto ref_out = origin.transform(output_srs);
            for (auto& p : m.verts)
            {
                p = p - weemesh::vert_t(ref_out.x, ref_out.y, ref_out.z);
            }
        }

        auto color = style.color;
            //styles.meshColorFunction ? styles.meshColorFunction(feature) :
            //styles.meshStyle.color;

        for (auto& tri : m.triangles)
        {
            meshGeom.vertices.emplace_back(m.verts[tri.second.i0]);
            meshGeom.vertices.emplace_back(m.verts[tri.second.i1]);
            meshGeom.vertices.emplace_back(m.verts[tri.second.i2]);

            meshGeom.colors.emplace_back(color);
            meshGeom.colors.emplace_back(color);
            meshGeom.colors.emplace_back(color);

            meshGeom.normals.emplace_back(glm::dvec3(0, 0, 1)); // will be computed later
            meshGeom.normals.emplace_back(glm::dvec3(0, 0, 1));
            meshGeom.normals.emplace_back(glm::dvec3(0, 0, 1));

            meshGeom.uvs.emplace_back(glm::fvec2(0, 0));
            meshGeom.uvs.emplace_back(glm::fvec2(0, 0));
            meshGeom.uvs.emplace_back(glm::fvec2(0, 0));

            meshGeom.indices.emplace_back((std::uint32_t)meshGeom.vertices.size() - 3);
            meshGeom.indices.emplace_back((std::uint32_t)meshGeom.vertices.size() - 2);
            meshGeom.indices.emplace_back((std::uint32_t)meshGeom.vertices.size() - 1);
        }
    }

    //! Converts a polygon or multipolygon feature into mesh geometry, recursively
    //! splitting very large geodetic polygons before the local weemesh tessellation step.
    void compile_polygon_feature(const Feature& feature, const MeshStyle& style,
        const GeoPoint& origin, ElevationSession& clamper, const SRS& output_srs,
        MeshGeometry& meshGeom, unsigned depth = 0)
    {
        constexpr double max_angular_radius = 75.0;
        constexpr unsigned max_split_depth = 8;
        constexpr float boundary_max_span = 100000.0f;

        Feature geodetic_feature = feature;
        bool source_is_geodetic = geodetic_feature.srs.isGeodetic();
        if (!geodetic_feature.srs.isGeodetic())
        {
            geodetic_feature.transformInPlace(geodetic_feature.srs.geodeticSRS());
        }

        tessellate_polygon_edges(
            geodetic_feature.geometry,
            geodetic_feature.srs,
            geodetic_feature.interpolation,
            boundary_max_span);

        glm::dvec3 center;
        if (!get_geometry_geodetic_center(geodetic_feature.geometry, center))
            return;

        unwrap_geometry_longitudes(geodetic_feature.geometry, center.x);

        double angular_radius = max_angular_radius_degrees(geodetic_feature.geometry, center);

        if (depth == 0 && !source_is_geodetic && angular_radius <= max_angular_radius)
        {
            compile_polygon_feature_with_weemesh(feature, style, origin, clamper, output_srs, meshGeom);
            return;
        }

        if (depth >= max_split_depth || angular_radius <= max_angular_radius)
        {
            normalize_geometry_longitudes(geodetic_feature.geometry);
            geodetic_feature.dirtyExtent();
            compile_polygon_feature_with_weemesh(geodetic_feature, style, origin, clamper, output_srs, meshGeom);
            return;
        }

        Box bounds = get_geometry_bounds(geodetic_feature.geometry);
        if (!bounds.valid())
            return;

        int axis = bounds.width() >= bounds.height() ? 0 : 1;
        double split_value = axis == 0 ? bounds.center().x : bounds.center().y;

        Feature a = geodetic_feature;
        a.geometry = clip_geometry_to_halfplane(geodetic_feature.geometry, axis, split_value, true);

        Feature b = geodetic_feature;
        b.geometry = clip_geometry_to_halfplane(geodetic_feature.geometry, axis, split_value, false);

        if (get_geometry_bounds(a.geometry).valid())
        {
            a.dirtyExtent();
            compile_polygon_feature(a, style, origin, clamper, output_srs, meshGeom, depth + 1);
        }

        if (get_geometry_bounds(b.geometry).valid())
        {
            b.dirtyExtent();
            compile_polygon_feature(b, style, origin, clamper, output_srs, meshGeom, depth + 1);
        }
    }

    //! Copies a ring into the implicit-closure representation used by PolygonGeometry.
    PolygonPart::Ring copy_open_ring(const std::vector<glm::dvec3>& input)
    {
        auto size = input.size();
        if (size > 1u && input.front() == input.back())
            --size;
        return PolygonPart::Ring(input.begin(), input.begin() + size);
    }

    //! Converts a strongly typed polygon part back to Feature's polygon layout.
    Geometry make_feature_polygon(const PolygonPart& input)
    {
        Geometry output(Geometry::Type::Polygon);
        output.points = input.outer;
        output.parts.reserve(input.holes.size());
        for (const auto& ring : input.holes)
            output.parts.emplace_back(Geometry::Type::LineString, ring);
        return output;
    }

    //! Emits one planar PolygonPart as triangles. This is the local-coordinate
    //! counterpart to the georeferenced gnomonic compiler above.
    void compile_planar_polygon(
        const PolygonPart& polygon,
        const Color& color,
        MeshGeometry& meshGeom)
    {
        if (polygon.outer.size() < 3u)
            return;

        Geometry local_geom = make_feature_polygon(polygon);
        Box bounds;
        double average_z = 0.0;
        std::size_t point_count = 0u;
        local_geom.eachPart([&](const Geometry& ring)
        {
            bounds.expandBy(ring.points.begin(), ring.points.end());
            for (const auto& point : ring.points)
            {
                average_z += point.z;
                ++point_count;
            }
        });

        if (!bounds.valid() || bounds.width() <= 0.0 || bounds.height() <= 0.0)
            return;

        average_z = point_count > 0u ? average_z / static_cast<double>(point_count) : 0.0;

        // Keep the seed vertices just outside the polygon bounds. If a polygon
        // vertex is itself a bounding-box corner, seeding at the exact corner
        // would cause weemesh to retain the seed's averaged Z instead of the
        // source ring vertex's Z when the boundary is inserted.
        const double padding = std::max(bounds.width(), bounds.height()) * 1e-6;

        weemesh::mesh_t mesh;
        const int marker = 0;
        const glm::dvec3 corners[] = {
            { bounds.xmin - padding, bounds.ymin - padding, average_z },
            { bounds.xmax + padding, bounds.ymin - padding, average_z },
            { bounds.xmin - padding, bounds.ymax + padding, average_z },
            { bounds.xmax + padding, bounds.ymax + padding, average_z }
        };
        int indices[4];
        for (unsigned i = 0u; i < 4u; ++i)
            indices[i] = mesh.get_or_create_vertex_from_vec3(corners[i], marker);

        mesh.add_triangle(indices[0], indices[1], indices[2]);
        mesh.add_triangle(indices[1], indices[3], indices[2]);

        local_geom.eachPart([&](const Geometry& ring)
        {
            for (std::size_t i = 0u; i < ring.points.size(); ++i)
            {
                const auto j = (i + 1u) % ring.points.size();
                mesh.insert(weemesh::segment_t{ ring.points[i], ring.points[j] },
                    marker | mesh._has_elevation_marker);
            }
        });

        std::vector<weemesh::triangle_t*> outside;
        outside.reserve(mesh.triangles.size());
        for (auto& entry : mesh.triangles)
        {
            auto& triangle = entry.second;
            const auto center = (triangle.p0 + triangle.p1 + triangle.p2) * (1.0 / 3.0);
            if (!local_geom.contains(center.x, center.y))
                outside.emplace_back(&triangle);
        }
        for (auto* triangle : outside)
            mesh.remove_triangle(*triangle);

        for (const auto& entry : mesh.triangles)
        {
            const auto& triangle = entry.second;
            const glm::dvec3 p0 = mesh.verts[triangle.i0];
            const glm::dvec3 p1 = mesh.verts[triangle.i1];
            const glm::dvec3 p2 = mesh.verts[triangle.i2];
            auto normal = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(normal) > 0.0)
                normal = glm::normalize(normal);
            else
                normal = glm::dvec3(0.0, 0.0, 1.0);

            const auto base = static_cast<std::uint32_t>(meshGeom.vertices.size());
            meshGeom.vertices.insert(meshGeom.vertices.end(), { p0, p1, p2 });
            meshGeom.colors.insert(meshGeom.colors.end(), { color, color, color });
            meshGeom.normals.insert(meshGeom.normals.end(), {
                glm::fvec3(normal), glm::fvec3(normal), glm::fvec3(normal) });
            meshGeom.uvs.insert(meshGeom.uvs.end(), {
                glm::fvec2(0.0f), glm::fvec2(0.0f), glm::fvec2(0.0f) });
            meshGeom.indices.insert(meshGeom.indices.end(), { base, base + 1u, base + 2u });
        }
    }
}

#if 0
//! Legacy entity-building path that compiled features and wrote ECS components
//! directly into a registry. Currently disabled in favor of explicit geometry builders.
entt::entity
FeatureBuilder::generate(const SRS& output_srs, Registry& registry)
{
    Workspace ws;
    ws.lineGeom.topology = LineTopology::Segments;
    ws.lineGeom.srs = output_srs;
    ws.meshGeom.srs = output_srs;

    MeshStyle tempMeshStyle = styles.meshStyle;

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
            compile_feature_to_lines(feature, styles.lineStyle, origin, clamper, output_srs, ws.lineGeom);
        }

        else if (feature.geometry.type == Geometry::Type::Polygon ||
            feature.geometry.type == Geometry::Type::MultiPolygon)
        {
            if (styles.meshColorFunction)
            {                
                tempMeshStyle.color = styles.meshColorFunction(feature);
                compile_polygon_feature(feature, tempMeshStyle, origin, clamper, output_srs, ws.meshGeom);
            }
            else
            {
                compile_polygon_feature(feature, styles.meshStyle, origin, clamper, output_srs, ws.meshGeom);
            }
        }

        else
        {
            Log()->warn("FeatureBuilder no support for " + Geometry::typeToString(feature.geometry.type));
        }
    }

    entt::entity e = entity;

    if (!ws.empty())
    {
        registry.write([&](entt::registry& r)
            {
                if (e == entt::null)
                {
                    e = r.create();
                }

                if (!ws.lineGeom.points.empty())
                {
                    auto& style = r.emplace_or_replace<LineStyle>(e, std::move(styles.lineStyle));
                    auto& geom = r.emplace_or_replace<LineGeometry>(e, std::move(ws.lineGeom));
                    r.emplace_or_replace<Line>(e, geom, style);
                }

                if (!ws.meshGeom.vertices.empty())
                {
                    auto& style = r.emplace_or_replace<MeshStyle>(e, std::move(styles.meshStyle));
                    auto& geom = r.emplace_or_replace<MeshGeometry>(e, std::move(ws.meshGeom));
                    r.emplace_or_replace<Mesh>(e, geom, style);
                }
            });
    }

    return e;
}
#endif

//! Builds an ECS LineGeometry object from all line and multiline features using
//! the builder's output SRS, origin, optional color function, and elevation clamper.
void
FeatureBuilder::buildLineGeometry(const std::vector<Feature>& features, const LineStyle& style,
    LineGeometry& lineGeom)
{
    lineGeom.topology = LineTopology::Segments;
    lineGeom.srs = outputSRS;

    if (colorFunction)
    {
        LineStyle tempStyle = style;
        for (auto& feature : features)
        {
            tempStyle.color = colorFunction(feature);
            if (clamper.srs)
                clamper.srs = feature.srs;
            compile_feature_to_lines(feature, tempStyle, origin, clamper, outputSRS, lineGeom);
        }
    }
    else
    {
        for (auto& feature : features)
        {
            if (clamper.srs)
                clamper.srs = feature.srs;
            compile_feature_to_lines(feature, style, origin, clamper, outputSRS, lineGeom);
        }
    }
}

//! Builds an ECS MeshGeometry object from all polygon and multipolygon features
//! using the builder's output SRS, origin, optional color function, and elevation clamper.
void
FeatureBuilder::buildMeshGeometry(const std::vector<Feature>& features, const MeshStyle& style,
    MeshGeometry& meshGeom)
{
    meshGeom.srs = outputSRS;

    if (colorFunction)
    {
        MeshStyle tempStyle = style;
        for (auto& feature : features)
        {
            tempStyle.color = colorFunction(feature);
            if (clamper.srs)
                clamper.srs = feature.srs;
            compile_polygon_feature(feature, tempStyle, origin, clamper, outputSRS, meshGeom);
        }
    }
    else
    {
        for (auto& feature : features)
        {
            if (clamper.srs)
                clamper.srs = feature.srs;
            compile_polygon_feature(feature, style, origin, clamper, outputSRS, meshGeom);
        }
    }
}

//! Builds an ECS PolygonGeometry object while retaining each polygon's exterior
//! and hole rings for consumers that do not require triangulation.
void
FeatureBuilder::buildPolygonGeometry(const std::vector<Feature>& features,
    const PolygonStyle& style, PolygonGeometry& polygonGeom)
{
    polygonGeom.srs = outputSRS;

    // Match MeshStyle's zero-value convention while retaining the boundary
    // curvature limit used by PolygonGeometry before this hint lived in style.
    const float boundaryResolution =
        style.resolution > 0.0f ? style.resolution : 100000.0f;

    for (const auto& feature : features)
    {
        if (feature.geometry.type != Geometry::Type::Polygon &&
            feature.geometry.type != Geometry::Type::MultiPolygon)
        {
            continue;
        }

        Geometry prepared = feature.geometry;
        tessellate_polygon_edges(
            prepared, feature.srs, feature.interpolation, boundaryResolution);

        if (clamper.srs)
            clamper.srs = feature.srs;

        prepared.eachPart([&](Geometry& ring)
        {
            if (clamper && !ring.points.empty())
                clamper.clampRange(ring.points.begin(), ring.points.end());
        });

        const auto feature_to_output = feature.srs.to(outputSRS);
        prepared.eachPart([&](Geometry& ring)
        {
            if (!ring.points.empty() && feature_to_output.valid())
                feature_to_output.transformArray(ring.points.data(), ring.points.size());
        });

        if (origin.valid())
        {
            const auto reference = origin.transform(outputSRS);
            if (reference.valid())
            {
                prepared.eachPart([&](Geometry& ring)
                {
                    for (auto& point : ring.points)
                        point -= glm::dvec3(reference.x, reference.y, reference.z);
                });
            }
        }

        const auto feature_color = colorFunction ? colorFunction(feature) : StockColor::White;
        Geometry::const_iterator(prepared, false).eachPart([&](const Geometry& polygon)
        {
            PolygonPart output;
            output.outer = copy_open_ring(polygon.points);
            if (output.outer.size() < 3u)
                return;

            output.holes.reserve(polygon.parts.size());
            for (const auto& hole : polygon.parts)
            {
                auto ring = copy_open_ring(hole.points);
                if (ring.size() >= 3u)
                    output.holes.emplace_back(std::move(ring));
            }

            polygonGeom.polygons.emplace_back(std::move(output));
            polygonGeom.colors.emplace_back(feature_color);
        });
    }
}

//! Triangulates PolygonGeometry for the ordinary mesh rendering pathway.
void
FeatureBuilder::buildMeshGeometry(const PolygonGeometry& polygons,
    const PolygonStyle& style, MeshGeometry& meshGeom)
{
    meshGeom.srs = polygons.srs;

    for (std::size_t i = 0u; i < polygons.polygons.size(); ++i)
    {
        const auto& polygon = polygons.polygons[i];
        const auto color = style.useGeometryColors && i < polygons.colors.size() ?
            polygons.colors[i] : style.color;

        if (!polygons.srs.valid())
        {
            compile_planar_polygon(polygon, color, meshGeom);
            continue;
        }

        Feature feature;
        feature.srs = polygons.srs;
        feature.geometry = make_feature_polygon(polygon);
        feature.dirtyExtent();

        MeshStyle meshStyle;
        meshStyle.color = color;
        meshStyle.resolution = style.resolution;
        ElevationSession no_clamper;
        compile_polygon_feature(
            feature, meshStyle, {}, no_clamper, polygons.srs, meshGeom);
    }
}
