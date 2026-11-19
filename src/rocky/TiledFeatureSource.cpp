/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "TiledFeatureSource.h"
#include "GDALFeatureSource.h"
#include "TileKey.h"
#include "Threading.h"
#include <map>
#include <set>
#include <atomic>
#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <cpl_vsi.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Accounts for coordinates across all multipart geometry members when enforcing aggregate query budgets.
    std::size_t vertexCount(const Geometry& geometry)
    {
        std::size_t count = geometry.points.size();
        for (const auto& part : geometry.parts) count += vertexCount(part);
        return count;
    }
}

struct TiledFeatureSource::Impl
{
    struct Cached { std::shared_ptr<const Batch> data; std::uint64_t used; };
    std::mutex mutex;
    detail::Gate<std::string> gates;
    std::map<std::string, Cached> cache;
    std::size_t bytes = 0;
    std::uint64_t clock = 0;
};

TiledFeatureSource::TiledFeatureSource() : _impl(new Impl()) { }
TiledFeatureSource::~TiledFeatureSource() = default;

Result<> TiledFeatureSource::validate() const
{
    if (tiles.empty() || !profile.valid() || profile.isComposite() || minZoom > maxZoom || maxZoom > 22)
        return Failure(Failure::ConfigurationError, "Invalid tiled feature URL/profile/zoom range");
    if (format != "MVT" && format != "FlatGeobuf")
        return Failure(Failure::ConfigurationError, "Tiled feature format must be MVT or FlatGeobuf");
    if (format == "MVT" && profile != Profile("spherical-mercator"))
        return Failure(Failure::ConfigurationError, "The GDAL MVT decoder requires a spherical-mercator source profile");
    if (wholeFeatures && (idField.empty() || format != "FlatGeobuf"))
        return Failure(Failure::ConfigurationError, "whole-features requires FlatGeobuf with an idField");
    for (const auto& uri : tiles)
        if (uri.empty() || uri.base().find("{z}") == std::string::npos ||
            uri.base().find("{x}") == std::string::npos || uri.base().find("{y}") == std::string::npos)
            return Failure(Failure::ConfigurationError, "Tiled feature URLs require {z}, {x}, and {y}");
    return ResultVoidOK;
}

FeatureSource::iterator TiledFeatureSource::iterate(const IOOptions&)
{
    struct Empty : iterator::implementation
    {
        //! Hierarchical sources cannot enumerate all levels without a query.
        bool hasMore() const override { return false; }
        //! Returns an unused empty feature; callers must check hasMore.
        Feature next() override { return {}; }
    };
    return iterator(new Empty());
}

Result<std::shared_ptr<const FeatureSource::Batch>> TiledFeatureSource::query(const Query& request, const IOOptions& io)
{
#ifndef ROCKY_HAS_GDAL
    return Failure_ServiceUnavailable;
#else
    auto valid = validate();
    if (valid.failed()) return valid.error();
    if (io.canceled()) return Failure_OperationCanceled;
    const auto extent = request.extent.transform(profile.srs());
    if (!extent.valid()) return Failure("Cannot transform tiled feature query into source profile");
    if (extent.crossesAntimeridian()) return Failure("Split wrapped tiled feature queries at the date line");
    auto result = std::make_shared<Batch>();
    const auto root = profile.extent();
    const double x0 = std::max(root.xmin(), extent.xmin()), x1 = std::min(root.xmax(), extent.xmax());
    const double y0 = std::max(root.ymin(), extent.ymin()), y1 = std::min(root.ymax(), extent.ymax());
    if (x1 <= x0 || y1 <= y0) return std::shared_ptr<const Batch>(result);
    const auto roots = profile.numTiles(0);
    const double ratio = std::min(root.width()/roots.x/(x1-x0), root.height()/roots.y/(y1-y0));
    const auto footprintLevel = static_cast<unsigned>(std::max(0.0, std::floor(std::log2(ratio))));
    unsigned level = std::clamp(std::min(request.level, footprintLevel), minZoom, maxZoom);
    int left, right, top, bottom;
    Profile::NumTiles dims;
    for (;;)
    {
        dims = profile.numTiles(level);
        const double width = root.width()/dims.x, height = root.height()/dims.y;
        left = std::clamp(int(std::floor((x0-root.xmin())/width)), 0, int(dims.x)-1);
        right = std::clamp(int(std::ceil((x1-root.xmin())/width))-1, 0, int(dims.x)-1);
        top = std::clamp(int(std::floor((root.ymax()-y1)/height)), 0, int(dims.y)-1);
        bottom = std::clamp(int(std::ceil((root.ymax()-y0)/height))-1, 0, int(dims.y)-1);
        if (std::int64_t(right-left+1)*(bottom-top+1) <= 64) break;
        if (level == minZoom) return Failure("Tiled feature query exceeds 64 source tiles");
        --level;
    }
    std::set<std::string> identities;
    std::size_t vertices = 0;
    for (int y = top; y <= bottom; ++y)
    for (int x = left; x <= right; ++x)
    {
        if (io.canceled()) return Failure_OperationCanceled;
        TileKey key(level, x, y, profile);
        std::string location = tiles.front().base();
        detail::replaceInPlace(location, "{z}", std::to_string(level));
        detail::replaceInPlace(location, "{x}", std::to_string(x));
        detail::replaceInPlace(location, "{y}", std::to_string(tms ? dims.y-1-y : y));
        URI uri(location, tiles.front().context());
        const auto identity = uri.full() + "|" + key.str();
        detail::ScopedGate<std::string> gate(_impl->gates, identity);
        std::shared_ptr<const Batch> decoded;
        {
            std::scoped_lock lock(_impl->mutex);
            auto found = _impl->cache.find(identity);
            if (found != _impl->cache.end()) { found->second.used = ++_impl->clock; decoded = found->second.data; }
        }
        if (!decoded)
        {
            auto response = uri.read(io);
            if (response.failed()) return response.error();
            if (response->content.data.size() > 32u*1024u*1024u) return Failure("Feature tile exceeds 32 MiB");
            static std::atomic_uint64_t serial{0};
            const auto path = "/vsimem/rocky-feature-" + std::to_string(++serial) +
                (format == "MVT" ? ".pbf" : ".fgb");
            auto file = VSIFileFromMemBuffer(path.c_str(),
                reinterpret_cast<GByte*>(response->content.data.data()), response->content.data.size(), FALSE);
            if (!file) return Failure("Cannot create feature tile memory file");
            VSIFCloseL(file);
            struct Cleanup
            {
                std::string path;
                //! Releases the borrowed memory file after its GDAL source closes.
                ~Cleanup() { VSIUnlink(path.c_str()); }
            } cleanup{path};
            auto source = GDALFeatureSource::create();
            source->uri = URI(path);
            source->ogrDriver = format;
            source->querySRS = dataSRS;
            source->spatialQueriesOnly = true;
            source->unindexedCacheBytes = request.maxBytes;
            if (format == "MVT")
            {
                source->layerName = sourceLayer;
                source->openOptions = { "X="+std::to_string(x), "Y="+std::to_string(y),
                    "Z="+std::to_string(level), "CLIP=NO", "METADATA_FILE=" };
            }
            auto opened = source->open();
            if (opened.failed()) return opened.error();
            Query all = request;
            all.extent = source->metadata().extent;
            if (!all.extent.valid()) all.extent = key.extent();
            auto data = source->query(all, io);
            if (data.failed()) return data.error();
            auto owned = std::make_shared<Batch>(*data.value());
            owned->bytes += sizeof(Batch) + 256;
            for (auto& record : owned->records)
            {
                record.collection = sourceLayer;
                if (!wholeFeatures) record.ownership = key.extent();
            }
            decoded = owned;
            if (io.canceled()) return Failure_OperationCanceled;
            std::scoped_lock lock(_impl->mutex);
            if (decoded->bytes <= decodedCacheBytes)
            {
                while (!_impl->cache.empty() && _impl->bytes + decoded->bytes > decodedCacheBytes)
                {
                    auto oldest = std::min_element(_impl->cache.begin(), _impl->cache.end(),
                        [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
                    _impl->bytes -= oldest->second.data->bytes;
                    _impl->cache.erase(oldest);
                }
                _impl->bytes += decoded->bytes;
                _impl->cache.emplace(identity, Impl::Cached{decoded, ++_impl->clock});
            }
        }
        result->bytes += decoded->bytes;
        if (result->bytes > request.maxBytes) return Failure("Tiled feature query exceeds byte budget");
        for (const auto& record : decoded->records)
        {
            const auto candidates = request.extent.transform(record.feature.srs);
            if (!candidates.valid()) return Failure("Cannot transform tiled feature candidate bounds");
            const auto& bounds = record.feature.extent;
            if (!candidates.crossesAntimeridian() && (bounds.xmax() < candidates.xmin() ||
                bounds.xmin() > candidates.xmax() || bounds.ymax() < candidates.ymin() ||
                bounds.ymin() > candidates.ymax())) continue;
            if (wholeFeatures)
            {
                const auto& id = record.feature.field(idField);
                if (!id.valid()) return Failure("whole-features tile has no stable idField value");
                const auto identity = std::to_string(id.index()) + ":" + id.stringValue();
                if (!identities.insert(identity).second) continue;
            }
            if (result->records.size() >= request.maxFeatures) return Failure("Tiled feature query exceeds feature budget");
            vertices += vertexCount(record.feature.geometry);
            if (vertices > request.maxVertices) return Failure("Tiled feature query exceeds vertex budget");
            result->records.push_back(record);
        }
    }
    return std::shared_ptr<const Batch>(result);
#endif
}
