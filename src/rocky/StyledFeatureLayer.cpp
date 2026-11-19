/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "StyledFeatureLayer.h"
#include "json.h"
#include "Threading.h"
#include "Context.h"
#include "GDALFeatureSource.h"
#include "TiledFeatureSource.h"
#include <limits>
#include <map>
#include <set>
#include <cmath>
#include <cstring>
#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <ogr_api.h>
#include <cpl_vsi.h>
#include <cpl_error.h>
#endif

using namespace ROCKY_NAMESPACE;

ROCKY_ADD_OBJECT_FACTORY(StyledFeature,
    [](std::string_view JSON, const IOOptions& io) {
        return StyledFeatureLayer::create(JSON, io); })

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double radius = 6378137.0, halfWorld = pi * radius, world = halfWorld * 2;
    constexpr double maxLatitude = 85.0511287798066;
    //! Spherical Mercator transforms retain unwrapped longitude across the date line.
    double mercatorY(double lat)
    {
        return radius * std::log(std::tan(pi / 4 + std::clamp(lat, -maxLatitude, maxLatitude) * pi / 360));
    }
    //! Inverse Mercator latitude in degrees for destination UV reprojection.
    double latitude(double y)
    {
        return (2 * std::atan(std::exp(y / radius)) - pi / 2) * 180 / pi;
    }
#ifdef ROCKY_HAS_GDAL
    struct GeometryDeleter
    {
        //! Releases the geometry owned by a worker-local smart pointer.
        void operator()(void* p) const
        {
            if (p)
                OGR_G_DestroyGeometry(p);
        }
    };
    using Geom = std::unique_ptr<void, GeometryDeleter>;
    //! Rectangle clip geometry in source Mercator meters.
    Geom rectangle(double x0, double y0, double x1, double y1)
    {
        Geom ring(OGR_G_CreateGeometry(wkbLinearRing)), polygon(OGR_G_CreateGeometry(wkbPolygon));
        OGR_G_AddPoint_2D(ring.get(), x0, y0);
        OGR_G_AddPoint_2D(ring.get(), x1, y0);
        OGR_G_AddPoint_2D(ring.get(), x1, y1);
        OGR_G_AddPoint_2D(ring.get(), x0, y1);
        OGR_G_AddPoint_2D(ring.get(), x0, y0);
        OGR_G_AddGeometryDirectly(polygon.get(), ring.release());
        return polygon;
    }
    //! Shift a private geometry clone into the destination's unwrapped world copy.
    void shiftX(OGRGeometryH g, double offset)
    {
        for (int i = 0; i < OGR_G_GetPointCount(g); ++i)
            OGR_G_SetPoint_2D(g, i, OGR_G_GetX(g, i) + offset, OGR_G_GetY(g, i));
        for (int i = 0; i < OGR_G_GetGeometryCount(g); ++i)
            shiftX(OGR_G_GetGeometryRef(g, i), offset);
    }
    //! Flatten polygon/line collections into a homogeneous batch, preserving polygon holes.
    void collect(OGRGeometryH input, OGRGeometryH output, bool polygons)
    {
        const auto type = wkbFlatten(OGR_G_GetGeometryType(input));
        if ((polygons && type == wkbPolygon) || (!polygons && (type == wkbLineString || type == wkbLinearRing)))
            OGR_G_AddGeometry(output, input);
        else
            for (int i = 0; i < OGR_G_GetGeometryCount(input); ++i)
                collect(OGR_G_GetGeometryRef(input, i), output, polygons);
    }
    struct SourceFeature
    {
        Feature feature; // typed properties; geometry represented by immutable WKB below
        std::string layer;
        std::vector<unsigned char> wkb;
        OGREnvelope bounds{};
        std::vector<unsigned char> ownership;
    };
    struct SourceTile
    {
        std::vector<SourceFeature> features;
        std::size_t bytes = 0;
    };
    //! Decode one XYZ MVT into immutable records. Each GDAL dataset is confined to this call/thread.
    Result<std::shared_ptr<const SourceTile>> decode(const std::string& bytes, const TileKey& key)
    {
        static std::atomic_uint64_t serial{0};
        const auto path = "/vsimem/rocky-style-" + std::to_string(++serial) + ".pbf";
        auto file =
            VSIFileFromMemBuffer(path.c_str(), reinterpret_cast<GByte*>(const_cast<char*>(bytes.data())), bytes.size(), FALSE);
        if (!file)
            return Failure("Cannot create MVT memory file");
        VSIFCloseL(file);
        struct Cleanup
        {
            std::string path;
            ~Cleanup()
            {
                VSIUnlink(path.c_str());
            }
        } cleanup{path};
        const std::string x = "X=" + std::to_string(key.x), y = "Y=" + std::to_string(key.y),
                          z = "Z=" + std::to_string(key.level);
        const char* options[] = {x.c_str(), y.c_str(), z.c_str(), "CLIP=NO", "METADATA_FILE=", nullptr};
        const char* drivers[] = {"MVT", nullptr};
        auto ds = GDALOpenEx(path.c_str(), GDAL_OF_VECTOR, drivers, options, nullptr);
        if (!ds)
            return Failure("Cannot decode MVT " + key.str());
        std::unique_ptr<void, decltype(&GDALClose)> dataset(ds, GDALClose);
        auto tile = std::make_shared<SourceTile>();
        for (int l = 0; l < GDALDatasetGetLayerCount(ds); ++l)
        {
            auto layer = GDALDatasetGetLayer(ds, l);
            while (auto handle = OGR_L_GetNextFeature(layer))
            {
                std::unique_ptr<void, decltype(&OGR_F_Destroy)> feature(handle, OGR_F_Destroy);
                auto g = OGR_F_GetGeometryRef(handle);
                if (!g || OGR_G_IsEmpty(g))
                    continue;
                auto type = wkbFlatten(OGR_G_GetGeometryType(g));
                if (type != wkbPolygon && type != wkbMultiPolygon && type != wkbLineString && type != wkbMultiLineString)
                    continue;
                // Quantization can collapse rings or introduce self-intersections in valid source polygons.
                // Repair once in the decoded cache, before GEOS clipping/union sees the geometry.
                Geom repaired;
                if (type == wkbPolygon || type == wkbMultiPolygon)
                {
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool valid = OGR_G_IsValid(g);
                    CPLPopErrorHandler();
                    if (!valid)
                    {
                        repaired.reset(OGR_G_MakeValid(g));
                        if (!repaired)
                            return Failure("Cannot repair quantized MVT polygon");
                        g = repaired.get();
                    }
                }
                SourceFeature record;
                record.layer = OGR_L_GetName(layer);
                record.feature.id = OGR_F_GetFID(handle);
                record.feature.geometry.type =
                    type == wkbPolygon || type == wkbMultiPolygon ? Geometry::Type::Polygon : Geometry::Type::LineString;
                OGR_G_GetEnvelope(g, &record.bounds);
                record.wkb.resize(OGR_G_WkbSize(g));
                OGR_G_ExportToWkb(g, wkbNDR, record.wkb.data());
                tile->bytes += record.wkb.size() + sizeof(SourceFeature) + record.layer.size();
                for (int i = 0; i < OGR_F_GetFieldCount(handle); ++i)
                {
                    if (!OGR_F_IsFieldSetAndNotNull(handle, i))
                        continue;
                    auto def = OGR_F_GetFieldDefnRef(handle, i);
                    auto& value = record.feature.fields[OGR_Fld_GetNameRef(def)];
                    auto type = OGR_Fld_GetType(def);
                    if (type == OFTInteger && OGR_Fld_GetSubType(def) == OFSTBoolean)
                        value.emplace<bool>(OGR_F_GetFieldAsInteger(handle, i) != 0);
                    else if (type == OFTInteger || type == OFTInteger64)
                        value.emplace<std::int64_t>(OGR_F_GetFieldAsInteger64(handle, i));
                    else if (type == OFTReal)
                        value.emplace<double>(OGR_F_GetFieldAsDouble(handle, i));
                    else
                    {
                        value.emplace<std::string>(OGR_F_GetFieldAsString(handle, i));
                        tile->bytes += value.stringValue().size();
                    }
                    tile->bytes += 96 + std::strlen(OGR_Fld_GetNameRef(def));
                }
                tile->features.push_back(std::move(record));
                if (tile->bytes > 128u * 1024u * 1024u)
                    return Failure("Decoded MVT exceeds 128 MiB");
            }
        }
        return std::shared_ptr<const SourceTile>(tile);
    }

    //! Convert buffered/clipped Mercator polygons to destination UV, adaptively subdividing nonlinear edges.
    //! Only geodetic and Mercator destination grids are accepted; no implicit reprojection approximation.
    void appendPolygons(OGRGeometryH g, VectorTile::Shape& shape, const GeoExtent& dst, bool geographic, std::size_t& count)
    {
        auto type = wkbFlatten(OGR_G_GetGeometryType(g));
        if (type != wkbPolygon)
        {
            for (int i = 0; i < OGR_G_GetGeometryCount(g); ++i)
                appendPolygons(OGR_G_GetGeometryRef(g, i), shape, dst, geographic, count);
            return;
        }
        std::vector<std::vector<glm::dvec2>> polygon;
        auto project = [&](glm::dvec2 p)
        {
            if (geographic)
                p = {p.x / radius * 180 / pi, latitude(p.y)};
            return glm::dvec2((p.x - dst.xmin()) / dst.width(), (p.y - dst.ymin()) / dst.height());
        };
        for (int r = 0; r < OGR_G_GetGeometryCount(g); ++r)
        {
            auto ring = OGR_G_GetGeometryRef(g, r);
            const int n = OGR_G_GetPointCount(ring);
            if (n < 4)
                continue;
            std::vector<glm::dvec2> points;
            auto put = [&](glm::dvec2 p)
            {
                if (++count > 1000000u)
                    throw std::runtime_error("Styled vector tile exceeds one million points");
                if (points.empty() || points.back() != p)
                    points.push_back(p);
            };
            // UV midpoint error below 1/4096 keeps reprojection subpixel at normal terrain tile sizes.
            std::function<void(glm::dvec2, glm::dvec2, unsigned)> edge;
            edge = [&](glm::dvec2 a, glm::dvec2 b, unsigned depth)
            {
                const auto pa = project(a), pb = project(b), mid = project((a + b) * 0.5);
                if (geographic && depth < 16 && glm::length(mid - (pa + pb) * 0.5) > 1.0 / 4096.0)
                {
                    edge(a, (a + b) * 0.5, depth + 1);
                    edge((a + b) * 0.5, b, depth + 1);
                }
                else
                    put(pb);
            };
            glm::dvec2 previous(OGR_G_GetX(ring, 0), OGR_G_GetY(ring, 0));
            put(project(previous));
            for (int i = 1; i < n; ++i)
            {
                glm::dvec2 next(OGR_G_GetX(ring, i), OGR_G_GetY(ring, i));
                edge(previous, next, 0);
                previous = next;
            }
            if (points.size() > 1 && points.front() == points.back())
                points.pop_back();
            if (points.size() >= 3)
                polygon.push_back(std::move(points));
        }
        if (!polygon.empty())
            shape.polygons.push_back(std::move(polygon));
    }

    //! Worker-local Mercator-to-tile transform. Geometry stays analytic after reprojection.
    struct TileProjection
    {
        GeoExtent extent;
        SRSOperation operation;
        SRS sourceSRS;
        const IOOptions& io;
        std::size_t points = 0;

        //! Captures a worker-local operation and cancellation context for one destination tile.
        TileProjection(const GeoExtent& dst, const IOOptions& in_io,
            const SRS& source = SRS::SPHERICAL_MERCATOR) :
            extent(dst), operation(source.to(dst.srs())), sourceSRS(source), io(in_io) { }

        //! Returns finite tile UVs, rejecting projection-domain failures instead of joining across them.
        glm::dvec2 project(glm::dvec2 p) const
        {
            if (!operation.transform(p.x, p.y) || !std::isfinite(p.x) || !std::isfinite(p.y))
                throw std::runtime_error("MVT geometry is outside the destination projection domain");
            return { (p.x - extent.xmin()) / extent.width(), (p.y - extent.ymin()) / extent.height() };
        }

        //! Subdivides edges to 1/8192 tile UV, checking quarter points as well as the midpoint.
        void edge(OGRGeometryH output, glm::dvec2 a, glm::dvec2 b, unsigned depth)
        {
            if (io.canceled()) throw std::runtime_error("MVT reprojection canceled");
            const auto pa = project(a), pb = project(b);
            double error = 0.0;
            for (double t : { 0.25, 0.5, 0.75 })
                error = std::max(error, glm::length(project(a + (b-a)*t) - (pa + (pb-pa)*t)));
            if (error > 1.0/8192.0)
            {
                if (depth >= 20) throw std::runtime_error("MVT projection edge cannot meet subdivision tolerance");
                edge(output, a, (a+b)*0.5, depth+1);
                edge(output, (a+b)*0.5, b, depth+1);
            }
            else
            {
                if (++points > 1000000u) throw std::runtime_error("Reprojected MVT exceeds one million points");
                OGR_G_AddPoint_2D(output, pb.x, pb.y);
            }
        }

        //! Reprojects private geometry recursively, retaining polygon exterior/interior associations.
        Geom geometry(OGRGeometryH input)
        {
            const auto type = std::strcmp(OGR_G_GetGeometryName(input), "LINEARRING") == 0 ?
                wkbLinearRing : OGR_G_GetGeometryType(input);
            Geom output(OGR_G_CreateGeometry(type));
            const int n = OGR_G_GetPointCount(input);
            if (n > 0)
            {
                glm::dvec2 previous(OGR_G_GetX(input, 0), OGR_G_GetY(input, 0));
                const auto p = project(previous);
                OGR_G_AddPoint_2D(output.get(), p.x, p.y);
                for (int i = 1; i < n; ++i)
                {
                    const glm::dvec2 next(OGR_G_GetX(input, i), OGR_G_GetY(input, i));
                    edge(output.get(), previous, next, 0);
                    previous = next;
                }
            }
            for (int i = 0; i < OGR_G_GetGeometryCount(input); ++i)
            {
                auto child = geometry(OGR_G_GetGeometryRef(input, i));
                if (OGR_G_AddGeometryDirectly(output.get(), child.get()) != OGRERR_NONE)
                    throw std::runtime_error("Cannot assemble reprojected MVT geometry");
                child.release();
            }
            return output;
        }

        //! GPU round strokes require a nearly uniform similarity in UV; otherwise buffer in source meters.
        double strokeScale() const
        {
            auto toMercator = extent.srs().to(sourceSRS);
            double scale = 0.0;
            for (double u : { 0.0, 0.5, 1.0 })
            for (double v : { 0.0, 0.5, 1.0 })
            {
                glm::dvec3 p(extent.xmin()+u*extent.width(), extent.ymin()+v*extent.height(), 0);
                if (!toMercator.transform(p, p)) return 0.0;
                const auto center = project(glm::dvec2(p));
                const auto x = project(glm::dvec2(p)+glm::dvec2(1,0)) - center;
                const auto y = project(glm::dvec2(p)+glm::dvec2(0,1)) - center;
                const double sx = glm::length(x), sy = glm::length(y), s = (sx+sy)*0.5;
                if (!(s > 0) || std::abs(sx-sy) > 0.01*s || std::abs(glm::dot(x,y)) > 0.01*sx*sy)
                    return 0.0;
                if (scale > 0 && std::abs(s-scale) > 0.01*scale) return 0.0;
                scale = s;
            }
            return scale;
        }
    };

    //! Counts vertices before spatial partitioning; includes interiors and collection members.
    std::size_t coordinateCount(OGRGeometryH geometry)
    {
        std::size_t count = OGR_G_GetPointCount(geometry);
        for (int i = 0; i < OGR_G_GetGeometryCount(geometry); ++i)
            count += coordinateCount(OGR_G_GetGeometryRef(geometry, i));
        return count;
    }

    //! Converts Rocky geometry into private OGR geometry, preserving polygon holes and multipart structure.
    Geom featureGeometry(const Geometry& geometry)
    {
        const auto type = geometry.type;
        if (type == Geometry::Type::Polygon)
        {
            Geom polygon(OGR_G_CreateGeometry(wkbPolygon));
            auto ring = [&](const Geometry& contour) {
                Geom output(OGR_G_CreateGeometry(wkbLinearRing));
                for (const auto& p : contour.points) OGR_G_AddPoint_2D(output.get(), p.x, p.y);
                OGR_G_CloseRings(output.get());
                if (OGR_G_AddGeometry(polygon.get(), output.get()) != OGRERR_NONE)
                    throw std::runtime_error("Cannot assemble polygon ring");
            };
            ring(geometry);
            for (const auto& hole : geometry.parts) ring(hole);
            return polygon;
        }
        const auto ogrType = type == Geometry::Type::LineString ? wkbLineString :
            type == Geometry::Type::MultiLineString ? wkbMultiLineString :
            type == Geometry::Type::MultiPolygon ? wkbMultiPolygon : wkbMultiPoint;
        Geom output(OGR_G_CreateGeometry(ogrType));
        for (const auto& p : geometry.points) OGR_G_AddPoint_2D(output.get(), p.x, p.y);
        for (const auto& part : geometry.parts)
        {
            auto child = featureGeometry(part);
            if (OGR_G_AddGeometry(output.get(), child.get()) != OGRERR_NONE)
                throw std::runtime_error("Cannot assemble feature geometry");
        }
        return output;
    }

    //! Rescales private projected coordinates after adaptive subdivision in a normalized metric frame.
    void scaleGeometry(OGRGeometryH geometry, double scale)
    {
        for (int i = 0; i < OGR_G_GetPointCount(geometry); ++i)
            OGR_G_SetPoint_2D(geometry, i, OGR_G_GetX(geometry, i)*scale, OGR_G_GetY(geometry, i)*scale);
        for (int i = 0; i < OGR_G_GetGeometryCount(geometry); ++i)
            scaleGeometry(OGR_G_GetGeometryRef(geometry, i), scale);
    }

    //! Normalizes source features to the compiler's metric frame without changing source/cache geometry.
    std::shared_ptr<const SourceTile> normalizeFeatures(const FeatureSource::Batch& input,
        const std::string& collection, const SRS& frame, double tolerance, const IOOptions& io)
    {
        auto output = std::make_shared<SourceTile>();
        std::size_t points = 0;
        const double scale = std::max(1.0, tolerance*8192.0);
        const GeoExtent normalized(frame, 0, 0, scale, scale);
        for (const auto& item : input.records)
        {
            if (io.canceled()) throw std::runtime_error("Feature normalization canceled");
            const auto type = item.feature.geometry.type;
            if (type == Geometry::Type::Points || type == Geometry::Type::MultiPoints) continue;
            auto raw = featureGeometry(item.feature.geometry);
            TileProjection projection(normalized, io, item.feature.srs);
            auto geometry = projection.geometry(raw.get());
            points += coordinateCount(geometry.get());
            if (points > 1000000) throw std::runtime_error("Normalized features exceed one million coordinates");
            scaleGeometry(geometry.get(), scale);
            const bool polygon = type == Geometry::Type::Polygon || type == Geometry::Type::MultiPolygon;
            if (polygon && !OGR_G_IsValid(geometry.get()))
            {
                Geom repaired(OGR_G_MakeValid(geometry.get()));
                if (!repaired) throw std::runtime_error("Cannot repair feature polygon");
                geometry = std::move(repaired);
            }
            SourceFeature record;
            record.feature = item.feature;
            record.feature.geometry = Geometry(polygon ? Geometry::Type::Polygon : Geometry::Type::LineString);
            record.feature.srs = frame;
            record.layer = collection;
            OGR_G_GetEnvelope(geometry.get(), &record.bounds);
            record.wkb.resize(OGR_G_WkbSize(geometry.get()));
            OGR_G_ExportToWkb(geometry.get(), wkbNDR, record.wkb.data());
            if (item.ownership.valid())
            {
                const auto& e = item.ownership;
                auto box = rectangle(e.xmin(), e.ymin(), e.xmax(), e.ymax());
                TileProjection ownerProjection(normalized, io, e.srs());
                auto owner = ownerProjection.geometry(box.get());
                scaleGeometry(owner.get(), scale);
                record.ownership.resize(OGR_G_WkbSize(owner.get()));
                OGR_G_ExportToWkb(owner.get(), wkbNDR, record.ownership.data());
            }
            output->bytes += record.wkb.size() + record.ownership.size() + sizeof(record);
            if (output->bytes > 128u*1024u*1024u) throw std::runtime_error("Normalized feature data exceeds 128 MiB");
            output->features.push_back(std::move(record));
        }
        return output;
    }

    //! Adapts cached MVT geometry to a polar metric frame when a style also contains polar dataset sources.
    std::shared_ptr<const SourceTile> normalizeMercatorTile(const SourceTile& input, const GeoExtent& core,
        const GeoExtent& query, const SRS& frame, double tolerance, const IOOptions& io)
    {
        auto output = std::make_shared<SourceTile>();
        const double scale = std::max(1.0, tolerance*8192.0);
        TileProjection projection(GeoExtent(frame, 0, 0, scale, scale), io);
        auto clip = rectangle(query.xmin(), query.ymin(), query.xmax(), query.ymax());
        auto sourceCore = rectangle(core.xmin(), core.ymin(), core.xmax(), core.ymax());
        Geom owned(OGR_G_Intersection(sourceCore.get(), clip.get()));
        if (!owned || OGR_G_IsEmpty(owned.get())) return output;
        auto projectedCore = projection.geometry(owned.get());
        scaleGeometry(projectedCore.get(), scale);
        std::vector<unsigned char> ownership(OGR_G_WkbSize(projectedCore.get()));
        OGR_G_ExportToWkb(projectedCore.get(), wkbNDR, ownership.data());
        for (const auto& feature : input.features)
        {
            if (io.canceled()) throw std::runtime_error("MVT polar normalization canceled");
            OGRGeometryH raw = nullptr;
            if (OGR_G_CreateFromWkb(feature.wkb.data(), nullptr, &raw, int(feature.wkb.size())) != OGRERR_NONE)
                throw std::runtime_error("Invalid MVT geometry");
            Geom geometry(raw);
            Geom clipped(OGR_G_Intersection(geometry.get(), clip.get()));
            if (!clipped) throw std::runtime_error("Cannot clip polar source geometry");
            if (OGR_G_IsEmpty(clipped.get())) continue;
            auto projected = projection.geometry(clipped.get());
            scaleGeometry(projected.get(), scale);
            SourceFeature record = feature;
            record.ownership = ownership;
            OGR_G_GetEnvelope(projected.get(), &record.bounds);
            record.wkb.resize(OGR_G_WkbSize(projected.get()));
            OGR_G_ExportToWkb(projected.get(), wkbNDR, record.wkb.data());
            output->bytes += sizeof(record) + record.wkb.size() + record.ownership.size();
            if (output->bytes > 128u*1024u*1024u) throw std::runtime_error("Polar features exceed 128 MiB");
            output->features.push_back(std::move(record));
        }
        return output;
    }
#endif
} // namespace

struct StyledFeatureLayer::Impl
{
    MapboxStyle style;
    std::vector<MapboxStyle::Source> sources;
    struct Binding
    {
        std::string id, collection;
        std::shared_ptr<FeatureSource> source;
    };
    std::vector<Binding> bindings;
    std::string attribution;
    mutable std::mutex mutex;
    std::atomic_uint64_t reads{0}, hits{0}, tilesBuilt{0}, features{0};
    std::atomic_uint64_t gpuStrokePieces{0}, bufferedStrokeBatches{0};
#ifdef ROCKY_HAS_GDAL
    struct Cached
    {
        std::shared_ptr<const SourceTile> tile;
        std::uint64_t used;
    };
    mutable std::map<std::string, Cached> cache;
    mutable std::size_t bytes = 0;
    mutable std::uint64_t clock = 0;
    mutable detail::Gate<std::string> gate;

    //! Shares decoded tiles within this layer; raw content caching is provided by URI and Services.
    Result<std::shared_ptr<const SourceTile>> read(const std::string& url, const TileKey& key,
                                                   const StyledFeatureLayer& owner, const IOOptions& io)
    {
        const auto identity = url + "|" + key.str();
        detail::ScopedGate<std::string> guard(gate, identity);
        if (io.canceled())
            return Failure_OperationCanceled;
        {
            std::scoped_lock lock(mutex);
            auto found = cache.find(identity);
            if (found != cache.end())
            {
                ++hits;
                found->second.used = ++clock;
                return found->second.tile;
            }
        }
        auto response = URI(url).read(io);
        if (response.failed())
            return response.error();
        if (response->fromCache) ++hits;
        else ++reads;
        const auto& data = response->content.data;
        if (data.size() > 32u * 1024u * 1024u)
            return Failure("MVT response exceeds 32 MiB");
        auto decoded = decode(data, key);
        if (decoded.failed())
            return decoded.error();
        if (io.canceled())
            return Failure_OperationCanceled;
        {
            std::scoped_lock lock(mutex);
            if (decoded.value()->bytes <= owner.decodedCacheBytes)
            {
                while (!cache.empty() && bytes + decoded.value()->bytes > owner.decodedCacheBytes)
                {
                    auto oldest = std::min_element(cache.begin(), cache.end(),
                                                   [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
                    bytes -= oldest->second.tile->bytes;
                    cache.erase(oldest);
                }
                bytes += decoded.value()->bytes;
                cache.emplace(identity, Cached{decoded.value(), ++clock});
            }
        }
        return decoded;
    }
#endif
};

StyledFeatureLayer::StyledFeatureLayer()
{
    construct({}, {});
}

StyledFeatureLayer::StyledFeatureLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void StyledFeatureLayer::construct(std::string_view JSON, const IOOptions& io)
{
    setLayerTypeName("StyledFeature");
    _referrer = io.referrer.value_or("");
    const auto j = parse_json(JSON);
    if (j.status.failed()) { _configurationStatus = j.status; return; }
    // Reject invalid provider options before any source I/O, including unsigned integer wraparound.
    const auto invalid = [&](const char* key) {
        _configurationStatus = Failure(Failure::ConfigurationError, std::string("Invalid StyledFeature option: ") + key);
    };
    if (!j.is_object()) { invalid("expected object"); return; }
    if (j.contains("style"))
    {
        const auto& value = j.at("style");
        if (value.is_object() && value.contains("version") && value.contains("href"))
        { invalid("ambiguous style document and URI"); return; }
        if (value.is_object() && value.contains("version") && !value.contains("href"))
            styleJSON = value.dump();
        else if ((!value.is_string() && !(value.is_object() && value.contains("href") && value.at("href").is_string())) ||
            !get_to(j, "style", styleURI, io) || styleURI.empty())
        { invalid("style"); return; }
    }
    if (j.contains("sourceBindings"))
    {
        if (!j.at("sourceBindings").is_object()) { invalid("sourceBindings"); return; }
        sourceBindings = j.at("sourceBindings").dump();
    }
    for (const auto* key : { "decodedCacheBytes", "maximumSourceZoom" })
    {
        if (!j.contains(key)) continue;
        const auto& value = j.at(key);
        const auto maximum = std::string_view(key) == "maximumSourceZoom" ?
            std::uint64_t(22) : std::uint64_t(std::numeric_limits<std::size_t>::max());
        if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
            value.get<std::uint64_t>() > maximum)
        { invalid(key); return; }
    }
    get_to(j, "decodedCacheBytes", decodedCacheBytes);
    get_to(j, "maximumSourceZoom", maximumSourceZoom);
    for (const auto* key : { "forceBufferedStrokes", "automaticStyleZoom" })
        if (j.contains(key) && !j.at(key).is_boolean()) { invalid(key); return; }
    get_to(j, "forceBufferedStrokes", forceBufferedStrokes);
    get_to(j, "automaticStyleZoom", automaticStyleZoom);
    if (j.contains("styleZoom"))
    {
        double zoom = 0.0;
        if (!j.at("styleZoom").is_number() || !get_to(j, "styleZoom", zoom) || !std::isfinite(zoom))
        { invalid("styleZoom"); return; }
        setStyleZoom(zoom);
    }
}

std::string StyledFeatureLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    if (!styleJSON.empty()) j["style"] = json::parse(styleJSON);
    else set(j, "style", styleURI);
    if (!sourceBindings.empty()) j["sourceBindings"] = json::parse(sourceBindings);
    set(j, "decodedCacheBytes", decodedCacheBytes);
    set(j, "maximumSourceZoom", maximumSourceZoom);
    set(j, "forceBufferedStrokes", forceBufferedStrokes);
    set(j, "automaticStyleZoom", automaticStyleZoom);
    set(j, "styleZoom", styleZoom());
    return j.dump();
}
void StyledFeatureLayer::setStyleZoom(double zoom)
{
    if (!std::isfinite(zoom))
        return;
    const double bucket = std::round(std::clamp(zoom, 0.0, 22.0) * 4) / 4;
    if (_styleZoom.exchange(bucket) != bucket)
        dirty();
}
std::optional<double> StyledFeatureLayer::viewStyleZoom(double zoom) const
{
    return automaticStyleZoom && std::isfinite(zoom) ?
        std::round(std::clamp(zoom, 0.0, 22.0) * 4.0) / 4.0 : _styleZoom.load();
}
MapboxStyle StyledFeatureLayer::style() const
{
    std::shared_lock lock(layerStateMutex());
    return _impl ? _impl->style : MapboxStyle{};
}
std::string StyledFeatureLayer::attributionText() const
{
    std::shared_lock lock(layerStateMutex());
    return _impl ? _impl->attribution : std::string{};
}
StyledFeatureLayer::Stats StyledFeatureLayer::stats() const
{
    std::shared_lock stateLock(layerStateMutex());
    Stats result;
    if (_impl)
    {
        result.reads = _impl->reads;
        result.hits = _impl->hits;
        result.tilesBuilt = _impl->tilesBuilt;
        result.features = _impl->features;
        result.gpuStrokePieces = _impl->gpuStrokePieces;
        result.bufferedStrokeBatches = _impl->bufferedStrokeBatches;
#ifdef ROCKY_HAS_GDAL
        std::scoped_lock lock(_impl->mutex);
        result.cacheBytes = _impl->bytes;
        result.cachedTiles = _impl->cache.size();
#endif
    }
    return result;
}

Result<> StyledFeatureLayer::openImplementation(const IOOptions& io)
{
    if (_configurationStatus.failed()) return _configurationStatus.error();
    if (styleURI.empty() && styleJSON.empty())
        return Failure(Failure::ConfigurationError, "StyledFeature requires a style URI or inline document");
    if (!styleURI.empty() && !styleJSON.empty())
        return Failure(Failure::ConfigurationError, "Set either styleURI or styleJSON, not both");
    auto base = super::openImplementation(io);
    if (base.failed())
        return base;
#ifndef ROCKY_HAS_GDAL
    return Failure(Failure::ServiceUnavailable, "Styled MVT terrain requires GDAL");
#else
    try
    {
        GDALAllRegister();
        std::string document = styleJSON;
        if (document.empty())
        {
            auto read = styleURI.read(io);
            if (read.failed()) return read.error();
            document = read->content.data;
        }
        auto impl = std::make_shared<Impl>();
        auto loaded = impl->style.load(document);
        if (loaded.failed())
            return loaded;
        impl->sources = impl->style.sources();
        const auto bindings = sourceBindings.empty() ? json::object() : json::parse(sourceBindings);
        auto bindingIO = io;
        bindingIO.referrer = _referrer;
        if (!bindings.is_object()) return Failure("sourceBindings must be an object");
        for (const auto& entry : bindings.items())
        {
            auto found = std::find_if(impl->sources.begin(), impl->sources.end(),
                [&](const auto& s) { return s.id == entry.key(); });
            if (found == impl->sources.end()) return Failure("Unknown source binding: " + entry.key());
            const auto& config = entry.value();
            const auto type = config.at("type").get<std::string>();
            const std::set<std::string> options = type == "GDAL" ?
                std::set<std::string>{"type","uri","layer","srs","sourceLayer","unindexedCacheBytes"} :
                std::set<std::string>{"type","tiles","format","profile","srs","scheme","minZoom","maxZoom",
                    "sourceLayer","geometryMode","idField"};
            for (const auto& option : config.items())
                if (options.count(option.key()) == 0) return Failure("Unknown source binding option: " + option.key());
            Impl::Binding binding;
            binding.id = entry.key();
            binding.collection = config.value("sourceLayer", "");
            if ((found->type == "vector" && binding.collection.empty()) ||
                (found->type == "geojson" && !binding.collection.empty()))
                return Failure("sourceLayer binding does not match style source type: " + entry.key());
            if (type == "GDAL")
            {
                auto source = GDALFeatureSource::create();
                URI uri;
                if (!get_to(config, "uri", uri, bindingIO) || uri.empty())
                    return Failure("GDAL source binding requires uri");
                if (uri.isRemote())
                    return Failure("GDAL dataset bindings require local data; remote tile URLs use TiledFeatures");
                source->uri = uri;
                source->layerName = config.value("layer", "");
                if (config.contains("srs")) source->querySRS = SRS(config.at("srs").get<std::string>());
                if (config.contains("srs") && !source->querySRS.valid()) return Failure("Invalid GDAL source CRS");
                source->spatialQueriesOnly = true;
                if (config.contains("unindexedCacheBytes"))
                {
                    const auto& value = config.at("unindexedCacheBytes");
                    if (!value.is_number_unsigned() && (!value.is_number_integer() || value.get<std::int64_t>() < 0))
                        return Failure("Invalid unindexedCacheBytes");
                    source->unindexedCacheBytes = value.get<std::size_t>();
                }
                auto opened = source->open();
                if (opened.failed()) return opened;
                binding.source = source;
            }
            else if (type == "TiledFeatures")
            {
                auto source = TiledFeatureSource::create();
                source->format = config.value("format", "FlatGeobuf");
                source->profile = Profile(config.value("profile", "spherical-mercator"));
                const auto scheme = config.value("scheme", "xyz");
                if (scheme != "xyz" && scheme != "tms") return Failure("Unknown tile addressing scheme");
                source->tms = scheme == "tms";
                source->minZoom = config.value("minZoom", 0u);
                source->maxZoom = config.value("maxZoom", maximumSourceZoom);
                for (const auto* key : {"minZoom", "maxZoom"})
                    if (config.contains(key) && (!config.at(key).is_number_integer() ||
                        config.at(key).get<std::int64_t>() < 0 || config.at(key).get<std::uint64_t>() > 22))
                        return Failure("Invalid source zoom range");
                source->sourceLayer = binding.collection;
                source->decodedCacheBytes = decodedCacheBytes / std::max<std::size_t>(1, bindings.size());
                if (config.contains("srs")) source->dataSRS = SRS(config.at("srs").get<std::string>());
                if (config.contains("srs") && !source->dataSRS.valid()) return Failure("Invalid tiled feature CRS");
                const auto mode = config.value("geometryMode", "tile-clipped");
                if (mode != "tile-clipped" && mode != "whole-features") return Failure("Unknown geometryMode");
                source->wholeFeatures = mode == "whole-features";
                source->idField = config.value("idField", "");
                if (!config.at("tiles").is_array()) return Failure("tiles must be an array of URIs");
                for (const auto& tile : config.at("tiles"))
                {
                    URI uri;
                    const json holder = {{"uri", tile}};
                    if (!get_to(holder, "uri", uri, bindingIO) || uri.empty()) return Failure("Invalid tile URI");
                    source->tiles.push_back(uri);
                }
                auto valid = source->validate();
                if (valid.failed()) return valid;
                binding.source = source;
            }
            else return Failure("Unknown feature source type: " + type);
            impl->bindings.push_back(std::move(binding));
            impl->sources.erase(found);
        }
        for (auto& source : impl->sources)
        {
            if (source.type != "vector") return Failure("GeoJSON style sources currently require a sourceBinding: " + source.id);
            std::string referrer = styleURI.empty() ? _referrer : styleURI.full();
            if (!source.url.empty())
            {
                URI uri(source.url, referrer);
                auto metadata = uri.read(io);
                if (metadata.failed())
                    return metadata.error();
                auto j = json::parse(metadata.value().content.data);
                referrer = uri.full();
                if (j.value("scheme", "xyz") != "xyz")
                    return Failure("Only XYZ MVT sources are supported");
                source.tiles = j.at("tiles").get<std::vector<std::string>>();
                source.minZoom = j.value("minzoom", 0u);
                source.maxZoom = j.value("maxzoom", 14u);
                source.attribution = j.value("attribution", "");
            }
            if (source.tiles.empty() || source.minZoom > source.maxZoom || source.maxZoom > 22)
                return Failure("Invalid MVT source: " + source.id);
            for (auto& url : source.tiles)
                url = URI(url, referrer).full();
            impl->attribution += source.attribution + " ";
        }
        for (const auto& message : impl->style.diagnostics())
            Log()->warn("Map style: {}", message);
        Log()->info("Map style: {} supported paint layers", impl->style.layers().size());
        _impl = std::move(impl);
        return ResultVoidOK;
    }
    catch (const std::exception& e)
    {
        return Failure(std::string("Opening styled MVT layer: ") + e.what());
    }
#endif
}
void StyledFeatureLayer::closeImplementation()
{
    _impl.reset();
    super::closeImplementation();
}

Result<VectorTile> StyledFeatureLayer::createTileImplementation(
    const TileKey& key, const IOOptions& io, std::optional<double> styleZoom) const
{
#ifndef ROCKY_HAS_GDAL
    return Failure_ServiceUnavailable;
#else
    if (!_impl)
        return Failure_ResourceUnavailable;
    try
    {
        const auto dst = key.extent();
        // Keep the existing Mercator fast path. Generic polar datasets use a valid metric polar frame.
        SRS frame = SRS::SPHERICAL_MERCATOR;
        const auto geodetic = dst.transform(SRS::WGS84);
        if (!_impl->bindings.empty() && geodetic.valid())
        {
            if (geodetic.ymin() >= 0.0 && geodetic.ymax() > maxLatitude)
                frame = SRS("+proj=stere +lat_0=90 +lat_ts=70 +lon_0=0 +datum=WGS84 +units=m +no_defs");
            else if (geodetic.ymax() <= 0.0 && geodetic.ymin() < -maxLatitude)
                frame = SRS("+proj=stere +lat_0=-90 +lat_ts=-70 +lon_0=0 +datum=WGS84 +units=m +no_defs");
        }
        const bool polar = frame != SRS::SPHERICAL_MERCATOR;
        const bool geographic = !polar && dst.srs() == SRS::WGS84;
        const bool reproject = polar || (!geographic && dst.srs() != frame);
        const auto footprint = reproject ? dst.transform(SRS::WGS84) : dst;
        if (!footprint.valid() || !(footprint.width() > 0.0) || !(footprint.height() > 0.0))
            return Failure("Cannot determine the vector source footprint for the destination profile");
        TileProjection projection(dst, io, frame);
        const double gpuScale = reproject ? projection.strokeScale() : 0.0;
        const double zoom = styleZoom.value_or(_styleZoom.load());
        const double latitudeScale = polar ? std::max(0.01, std::cos(
            (geodetic.ymin()+geodetic.ymax())*0.5*pi/180.0)) : 1.0;
        const double pixel = latitudeScale * world / (512.0 * std::exp2(zoom));
        const auto metric = polar ? dst.transform(frame) : GeoExtent();
        if (polar && !metric.valid()) return Failure("Cannot determine polar feature query footprint");
        const bool lonlat = geographic || reproject;
        const double x0 = polar ? metric.xmin() : lonlat ? footprint.xmin() * pi / 180 * radius : dst.xmin();
        const double x1 = polar ? metric.xmax() : lonlat ? (footprint.xmin()+footprint.width()) * pi / 180 * radius : dst.xmax();
        const double y0 = polar ? metric.ymin() : lonlat ? mercatorY(footprint.ymin()) : dst.ymin();
        const double y1 = polar ? metric.ymax() : lonlat ? mercatorY(footprint.ymax()) : dst.ymax();
        // Stable clipping within a source-zoom interval permits geometry reuse across paint updates.
        const double guard = 128 * latitudeScale * world / (512.0 * std::exp2(std::floor(zoom)));
        // General projections use canonical longitude copies. A full-world footprint must not wrap twice.
        const double clipX0 = !polar && reproject && footprint.width() >= 359.999 ? -halfWorld : x0-guard;
        const double clipX1 = !polar && reproject && footprint.width() >= 359.999 ? halfWorld : x1+guard;
        auto clip = rectangle(clipX0, polar ? y0-guard : std::max(-halfWorld, y0-guard),
            clipX1, polar ? y1+guard : std::min(halfWorld, y1+guard));
        struct Tile
        {
            std::string source;
            std::shared_ptr<const SourceTile> data;
            TileKey key;
            int wrap;
            GeoExtent extent;
        };
        std::vector<Tile> tiles;
        for (const auto& binding : _impl->bindings)
        {
            const bool visible = std::any_of(_impl->style.layers().begin(), _impl->style.layers().end(),
                [&](const auto& layer) { return layer.source == binding.id && zoom >= layer.minZoom && zoom < layer.maxZoom; });
            if (!visible) continue;
            FeatureSource::Query request;
            request.resolution = pixel;
            request.level = static_cast<unsigned>(std::floor(zoom));
            const int firstWrap = polar ? 0 : int(std::floor((clipX0+halfWorld)/world));
            const int lastWrap = polar ? 0 : int(std::ceil((clipX1+halfWorld)/world))-1;
            for (int wrap = firstWrap; wrap <= lastWrap; ++wrap)
            {
                request.extent = GeoExtent(frame, polar ? clipX0 : std::max(-halfWorld, clipX0-wrap*world),
                    polar ? y0-guard : std::max(-halfWorld, y0-guard),
                    polar ? clipX1 : std::min(halfWorld, clipX1-wrap*world),
                    polar ? y1+guard : std::min(halfWorld, y1+guard));
                auto queried = binding.source->query(request, io);
                if (queried.failed()) return queried.error();
                auto normalized = normalizeFeatures(*queried.value(), binding.collection, frame, pixel*0.1, io);
                tiles.push_back({binding.id, normalized, TileKey(), wrap, request.extent});
            }
        }
        Profile mercator("spherical-mercator");
        const double mx0 = polar ? geodetic.xmin()*pi/180.0*radius : x0;
        const double mx1 = polar ? (geodetic.xmin()+geodetic.width())*pi/180.0*radius : x1;
        const double my0 = polar ? mercatorY(geodetic.ymin()) : y0;
        const double my1 = polar ? mercatorY(geodetic.ymax()) : y1;
        const double mguard = 128 * world / (512.0 * std::exp2(std::floor(zoom)));
        const double mclip0 = polar ? std::max(-halfWorld, mx0-mguard) : clipX0;
        const double mclip1 = polar ? std::min(halfWorld, mx1+mguard) : clipX1;
        if (my1 > my0)
            for (const auto& source : _impl->sources)
            {
                int z = std::max(0, static_cast<int>(std::floor(std::log2(world / (mx1 - mx0)))));
                z = std::min({z, static_cast<int>(std::floor(zoom)), static_cast<int>(source.maxZoom),
                              static_cast<int>(maximumSourceZoom)});
                z = std::max(z, static_cast<int>(source.minZoom));
                int left, right, top, bottom, n;
                double span;
                for (;;)
                {
                    n = 1 << z;
                    span = world / n;
                    left = static_cast<int>(std::floor((mclip0 + halfWorld) / span));
                    right = static_cast<int>(std::ceil((mclip1 + halfWorld) / span)) - 1;
                    top = std::clamp(static_cast<int>(std::floor((halfWorld - my1 - mguard) / span)), 0, n - 1);
                    bottom = std::clamp(static_cast<int>(std::floor((halfWorld - my0 + mguard) / span)), 0, n - 1);
                    if (std::int64_t(right - left + 1) * (bottom - top + 1) <= 64)
                        break;
                    if (z <= static_cast<int>(source.minZoom))
                        return Failure("MVT source footprint exceeds 64 tiles");
                    --z;
                }
                for (int y = top; y <= bottom; ++y)
                    for (int x = left; x <= right; ++x)
                    {
                        if (io.canceled())
                            return Failure_OperationCanceled;
                        const int wrapped = (x % n + n) % n, wrap = (x - wrapped) / n;
                        TileKey sourceKey(z, wrapped, y, mercator);
                        std::string url = source.tiles.front();
                        detail::replaceInPlace(url, "{z}", std::to_string(z));
                        detail::replaceInPlace(url, "{x}", std::to_string(wrapped));
                        detail::replaceInPlace(url, "{y}", std::to_string(y));
                        auto tile = _impl->read(url, sourceKey, *this, io);
                        if (tile.failed())
                            return tile.error();
                        if (polar)
                        {
                            auto normalized = normalizeMercatorTile(*tile.value(), sourceKey.extent(),
                                GeoExtent(SRS::SPHERICAL_MERCATOR,mclip0,std::max(-halfWorld,my0-mguard),
                                    mclip1,std::min(halfWorld,my1+mguard)),frame,pixel*0.1,io);
                            tiles.push_back({source.id, normalized, TileKey(), 0,
                                GeoExtent(frame,clipX0,y0-guard,clipX1,y1+guard)});
                        }
                        else tiles.push_back({source.id, tile.value(), sourceKey, wrap});
                    }
            }
        VectorTile result;
        std::size_t points = 0;
        std::uint32_t paintGroup = 0u;
        for (std::size_t l = 0; l < _impl->style.layers().size(); ++l)
        {
            const auto& layer = _impl->style.layers()[l];
            if (zoom < layer.minZoom || zoom >= layer.maxZoom)
                continue;
            if (layer.type == MapboxStyle::Type::Background)
            {
                auto paint = _impl->style.evaluate(l, {}, zoom);
                if (paint.failed())
                    return paint.error();
                if (paint.value())
                {
                    VectorTile::Shape shape;
                    shape.color = paint.value()->color;
                    shape.contours = {{{-0.01, -0.01}, {1.01, -0.01}, {1.01, 1.01}, {-0.01, 1.01}}};
                    result.shapes.push_back(std::move(shape));
                }
                continue;
            }
            struct Batch
            {
                MapboxStyle::Paint paint;
                Geom geometry;
            };
            std::vector<Batch> batches;
            const bool fill = layer.type == MapboxStyle::Type::Fill;
            for (const auto& tile : tiles)
            {
                if (tile.source != layer.source)
                    continue;
                const auto bounds = tile.key.valid() ? tile.key.extent() : tile.extent;
                const double offset = tile.wrap * world;
                auto owned = rectangle(bounds.xmin() + offset, bounds.ymin(), bounds.xmax() + offset, bounds.ymax());
                Geom footprint(OGR_G_Intersection(owned.get(), clip.get()));
                if (!footprint || OGR_G_IsEmpty(footprint.get()))
                    continue;
                OGREnvelope envelope;
                OGR_G_GetEnvelope(footprint.get(), &envelope);
                for (const auto& record : tile.data->features)
                {
                    if (io.canceled())
                        return Failure_OperationCanceled;
                    if (record.layer != layer.sourceLayer || record.bounds.MaxX + offset < envelope.MinX ||
                        record.bounds.MinX + offset > envelope.MaxX || record.bounds.MaxY < envelope.MinY ||
                        record.bounds.MinY > envelope.MaxY)
                        continue;
                    if (fill && record.feature.geometry.type != Geometry::Type::Polygon)
                        continue;
                    auto evaluated = _impl->style.evaluate(l, record.feature, zoom);
                    if (evaluated.failed())
                        return evaluated.error();
                    if (!evaluated.value())
                        continue;
                    const auto paint = *evaluated.value();
                    if (paint.widthPixels * 2 + paint.gapPixels > 256)
                        return Failure("Style line exceeds the 128-pixel guard radius");
                    OGRGeometryH raw = nullptr;
                    if (OGR_G_CreateFromWkb(record.wkb.data(), nullptr, &raw, static_cast<int>(record.wkb.size())) !=
                        OGRERR_NONE)
                        return Failure("Invalid cached MVT geometry");
                    Geom geometry(raw);
                    if (offset != 0)
                        shiftX(raw, offset);
                    // Polygon outlines use the original rings, before ownership clipping introduces new edges.
                    if (!fill && record.feature.geometry.type == Geometry::Type::Polygon)
                        geometry.reset(OGR_G_Boundary(geometry.get()));
                    Geom ownedFootprint;
                    if (!record.ownership.empty())
                    {
                        OGRGeometryH owner = nullptr;
                        if (OGR_G_CreateFromWkb(record.ownership.data(), nullptr, &owner,
                            static_cast<int>(record.ownership.size())) != OGRERR_NONE)
                            return Failure("Invalid feature ownership geometry");
                        Geom ownership(owner);
                        if (offset != 0.0) shiftX(ownership.get(), offset);
                        ownedFootprint.reset(OGR_G_Intersection(ownership.get(), footprint.get()));
                        if (!ownedFootprint) return Failure("Cannot clip feature ownership");
                    }
                    Geom clipped(OGR_G_Intersection(geometry.get(), ownedFootprint ? ownedFootprint.get() : footprint.get()));
                    if (!clipped)
                        return Failure("MVT geometry clipping failed");
                    if (OGR_G_IsEmpty(clipped.get()))
                        continue;
                    if (batches.empty() || batches.back().paint.color != paint.color ||
                        batches.back().paint.widthPixels != paint.widthPixels ||
                        batches.back().paint.gapPixels != paint.gapPixels)
                        batches.push_back({paint, Geom(OGR_G_CreateGeometry(fill ? wkbMultiPolygon : wkbMultiLineString))});
                    collect(clipped.get(), batches.back().geometry.get(), fill);
                    ++_impl->features;
                }
            }
            for (auto& batch : batches)
            {
                if (io.canceled())
                    return Failure_OperationCanceled;
                ++paintGroup;
                if (!fill && !forceBufferedStrokes && batch.paint.gapPixels == 0.0 && (!reproject || gpuScale > 0.0))
                {
                    // Retain centerlines in an isotropic Mercator frame. Chunking shares endpoints;
                    // the shader unions coverage before blending, including translucent intersections.
                    const double centerY = geographic ? mercatorY((dst.ymin() + dst.ymax()) * 0.5) :
                        (y0 + y1) * 0.5;
                    VectorTile::Shape shape;
                    shape.type = VectorTile::Shape::Type::Line;
                    shape.coordinates = reproject ? VectorTile::Shape::Coordinates::TileUV :
                        VectorTile::Shape::Coordinates::Mercator;
                    shape.color = batch.paint.color;
                    shape.width = static_cast<float>(batch.paint.widthPixels * pixel * (reproject ? gpuScale : 1.0/(x1-x0)));
                    shape.coverageGroup = paintGroup;
                    std::size_t chunkPoints = 0;
                    // Flush bounded centerline batches without producing any stroke polygons.
                    auto flush = [&]() {
                        if (shape.contours.empty()) return;
                        result.shapes.push_back(shape);
                        shape.contours.clear();
                        chunkPoints = 0;
                        ++_impl->gpuStrokePieces;
                        if (result.shapes.size() > 4096u)
                            throw std::runtime_error("MVT tile exceeds 4096 coverage pieces");
                    };
                    Geom projectedLines;
                    auto paths = batch.geometry.get();
                    if (reproject)
                    {
                        auto transformed = projection.geometry(paths);
                        const double padding = std::max(0.02, double(shape.width));
                        auto tileClip = rectangle(-padding, -padding, 1+padding, 1+padding);
                        Geom clipped(OGR_G_Intersection(transformed.get(), tileClip.get()));
                        if (!clipped) return Failure("Projected MVT stroke clipping failed");
                        projectedLines.reset(OGR_G_CreateGeometry(wkbMultiLineString));
                        collect(clipped.get(), projectedLines.get(), false);
                        paths = projectedLines.get();
                    }
                    for (int c = 0; c < OGR_G_GetGeometryCount(paths); ++c)
                    {
                        auto path = OGR_G_GetGeometryRef(paths, c);
                        const int n = OGR_G_GetPointCount(path);
                        for (int first = 0; first + 1 < n;)
                        {
                            if (chunkPoints >= 256u) flush();
                            const int last = std::min(n - 1, first + static_cast<int>(257u - chunkPoints) - 1);
                            std::vector<glm::dvec2> contour;
                            for (int p = first; p <= last; ++p)
                            {
                                if (reproject) contour.emplace_back(OGR_G_GetX(path, p), OGR_G_GetY(path, p));
                                else contour.emplace_back((OGR_G_GetX(path, p) - x0) / (x1 - x0),
                                    (OGR_G_GetY(path, p) - centerY) / (x1 - x0));
                                if (++points > 1000000u)
                                    throw std::runtime_error("Styled vector tile exceeds one million points");
                            }
                            chunkPoints += contour.size();
                            shape.contours.push_back(std::move(contour));
                            first = last;
                        }
                    }
                    flush();
                    continue;
                }
                Geom coverage;
                if (fill)
                {
                    // Dissolve tile-core edges and duplicate polygon coverage before analytic antialiasing.
                    coverage.reset(OGR_G_UnionCascaded(batch.geometry.get()));
                }
                else
                {
                    ++_impl->bufferedStrokeBatches;
                    const double reach = (batch.paint.gapPixels > 0 ? batch.paint.widthPixels + batch.paint.gapPixels / 2
                                                                    : batch.paint.widthPixels / 2) *
                                         pixel;
                    coverage.reset(OGR_G_Buffer(batch.geometry.get(), reach, 6));
                    if (coverage && batch.paint.gapPixels > 0)
                    {
                        Geom inside(OGR_G_Buffer(batch.geometry.get(), batch.paint.gapPixels / 2 * pixel, 6));
                        if (!inside)
                            return Failure("MVT inner stroke buffering failed");
                        coverage.reset(OGR_G_Difference(coverage.get(), inside.get()));
                    }
                }
                if (!coverage)
                    return Failure("MVT fill union or stroke buffering failed");
                if (reproject)
                {
                    // Buffer in source space first, then restrict the footprint before projecting.
                    // This keeps wide source guards away from the far-side singularities of cube faces.
                    const double fringe = std::max(x1-x0, y1-y0)*0.01;
                    const bool canonical = !polar && x0 >= -halfWorld && x1 <= halfWorld;
                    auto sourceClip = rectangle(canonical ? std::max(-halfWorld, x0-fringe) : x0-fringe,
                        polar ? y0-fringe : std::max(-halfWorld, y0-fringe),
                        canonical ? std::min(halfWorld, x1+fringe) : x1+fringe,
                        polar ? y1+fringe : std::min(halfWorld, y1+fringe));
                    Geom local(OGR_G_Intersection(coverage.get(), sourceClip.get()));
                    if (!local) return Failure("MVT projection footprint clipping failed");
                    auto transformed = projection.geometry(local.get());
                    auto tileClip = rectangle(-0.02, -0.02, 1.02, 1.02);
                    coverage.reset(OGR_G_Intersection(transformed.get(), tileClip.get()));
                    if (!coverage) return Failure("Projected MVT polygon clipping failed");
                }
                // Spatially split connected road networks before atlas construction. Disjoint
                // pieces share a coverage group, so artificial edges do not darken translucent paint.
                std::function<void(OGRGeometryH, unsigned)> partition;
                partition = [&](OGRGeometryH geometry, unsigned depth)
                {
                    if (io.canceled())
                        throw std::runtime_error("MVT tile canceled");
                    if (OGR_G_IsEmpty(geometry))
                        return;
                    if (coordinateCount(geometry) <= 512u)
                    {
                        VectorTile::Shape shape;
                        shape.color = batch.paint.color;
                        shape.coverageGroup = paintGroup;
                        appendPolygons(geometry, shape,
                            reproject ? GeoExtent(SRS::SPHERICAL_MERCATOR, 0, 0, 1, 1) : dst,
                            !reproject && geographic, points);
                        if (!shape.polygons.empty())
                            result.shapes.push_back(std::move(shape));
                        if (result.shapes.size() > 4096u)
                            throw std::runtime_error("MVT tile exceeds 4096 coverage pieces");
                        return;
                    }
                    if (depth >= 16u)
                        throw std::runtime_error("MVT coverage cannot fit bounded spatial partitions");
                    OGREnvelope bounds;
                    OGR_G_GetEnvelope(geometry, &bounds);
                    const bool splitX = bounds.MaxX - bounds.MinX >= bounds.MaxY - bounds.MinY;
                    const double mid = splitX ? (bounds.MinX + bounds.MaxX) * 0.5 : (bounds.MinY + bounds.MaxY) * 0.5;
                    for (unsigned side = 0; side < 2u; ++side)
                    {
                        auto box = rectangle(splitX && side ? mid : bounds.MinX, !splitX && side ? mid : bounds.MinY,
                                             splitX && !side ? mid : bounds.MaxX, !splitX && !side ? mid : bounds.MaxY);
                        Geom piece(OGR_G_Intersection(geometry, box.get()));
                        if (!piece)
                            throw std::runtime_error("MVT coverage partition failed");
                        partition(piece.get(), depth + 1u);
                    }
                };
                partition(coverage.get(), 0u);
            }
        }
        ++_impl->tilesBuilt;
        return result;
    }
    catch (const std::exception& e)
    {
        if (io.canceled())
            return Failure_OperationCanceled;
        return Failure(std::string("Building styled MVT tile: ") + e.what());
    }
#endif
}
