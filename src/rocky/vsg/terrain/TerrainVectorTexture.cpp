/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TerrainVectorTexture.h"
#include "../ShaderDefines.h"
#include <rocky/Map.h>
#include <rocky/GeoPoint.h>
#ifdef ROCKY_HAS_SLUGHORN
#include "../ecs/slug/SlugAdapter.h"
#endif
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <mutex>

using namespace ROCKY_NAMESPACE;

#ifdef ROCKY_HAS_SLUGHORN
//! Weakly cached immutable atlas; paint snapshots own its lifetime. No Vulkan allocation occurs here.
struct rocky::TerrainVectorGeometry
{
    vsg::ref_ptr<vsg::DescriptorImage> curves, bands;
    std::vector<detail::SlugLayerOutput> layers;
    std::uint32_t textureWidthLog2 = 0, maxDimension = 0;
};
#endif

namespace
{
    constexpr double pi = 3.14159265358979323846, radius = 6378137.0;
    //! Clamped spherical Mercator northing, including tile centers at the geographic poles.
    double mercatorY(double degrees)
    {
        return radius * std::asinh(std::tan(std::clamp(degrees, -85.0511287798066, 85.0511287798066) * pi / 180.0));
    }
    //! Mercator origin and isotropic scale for a supported terrain extent.
    glm::dvec3 mercatorFrame(const GeoExtent& extent)
    {
        const bool geographic = extent.srs() == SRS::WGS84;
        return { geographic ? extent.xmin() * pi / 180.0 * radius : extent.xmin(),
            geographic ? mercatorY((extent.ymin() + extent.ymax()) * 0.5) : (extent.ymin() + extent.ymax()) * 0.5,
            geographic ? extent.width() * pi / 180.0 * radius : extent.width() };
    }
}

TerrainVectorRequest rocky::terrainVectorRequest(
    const Map& map, const TileKey& key, std::optional<double> viewStyleZoom)
{
    TerrainVectorRequest result;
    result.key = key;
    unsigned level = 0u;
    for (auto layer : map.layers<VectorTileLayer>())
    {
        detail::hashCombine(result.signature, std::hash<UID>{}(layer->uid()));
        detail::hashCombine(result.signature, std::hash<Revision>{}(layer->revision()));
        detail::hashCombine(result.signature, std::hash<float>{}(layer->opacity.value()));
        auto sourceKey = layer->isOpen() ? layer->bestAvailableTileKey(key) : TileKey{};
        detail::hashCombine(result.signature, sourceKey.valid() ? 1u : 0u);
        detail::hashCombine(result.signature, sourceKey.valid() ? sourceKey.level : 0u);
        if (sourceKey.valid())
        {
            level = std::max(level, sourceKey.level);
            auto zoom = viewStyleZoom ? layer->viewStyleZoom(*viewStyleZoom) : std::optional<double>{};
            detail::hashCombine(result.signature, zoom.has_value() ? 1u : 0u);
            if (zoom) detail::hashCombine(result.signature, std::hash<double>{}(*zoom));
            result.sources.push_back({ layer, sourceKey, layer->revision(), layer->opacity.value(), zoom });
        }
    }
    if (!result.sources.empty())
        result.key = key.createAncestorKey(level);
    return result;
}

Result<vsg::ref_ptr<TerrainVectorTexture>> rocky::buildTerrainVectorTexture(
    const TerrainVectorRequest& request, const IOOptions& io, std::uint32_t maxImageDimension)
{
#ifndef ROCKY_HAS_SLUGHORN
    return Failure(Failure::ServiceUnavailable, "Terrain vector textures require Slughorn");
#else
    using namespace detail;
    if (io.canceled())
        return Failure_OperationCanceled;
    SlugAtlasInput input;
    input.distanceStrokes = true;
    std::vector<glm::vec4> bounds;
    std::vector<glm::vec4> projections;
    std::vector<float> strokeWidths;
    std::vector<std::uint32_t> paintGroups;
    std::uint32_t paintGroup = 0u;
    for (const auto& source : request.sources)
    {
        auto tile = source.layer->createTile(source.key, io, source.styleZoom);
        if (tile.failed())
            return tile.error();
        if (source.layer->revision() != source.revision || io.canceled())
            return Failure_OperationCanceled;
        if (!std::isfinite(source.opacity) || source.opacity < 0.0f || source.opacity > 1.0f)
            return Failure(Failure::ConfigurationError, "Vector layer opacity must be in [0,1]");
        const auto src = source.key.extent();
        const auto dst = request.key.extent();
        const glm::dvec2 scale(src.width() / dst.width(), src.height() / dst.height());
        const glm::dvec2 bias((src.xmin() - dst.xmin()) / dst.width(), (src.ymin() - dst.ymin()) / dst.height());
        std::uint32_t previousGroup = 0u;
        for (const auto& shape : tile.value().shapes)
        {
            const bool mercator = shape.coordinates == VectorTile::Shape::Coordinates::Mercator;
            if (mercator && dst.srs() != SRS::WGS84 && dst.srs() != SRS::SPHERICAL_MERCATOR)
                return Failure("Mercator vector coordinates require geodetic or spherical Mercator terrain");
            const auto srcFrame = mercator ? mercatorFrame(src) : glm::dvec3(0);
            const auto dstFrame = mercator ? mercatorFrame(dst) : glm::dvec3(0);
            const auto shapeScale = mercator ? glm::dvec2(srcFrame.z / dstFrame.z) : scale;
            const auto shapeBias = mercator ? glm::dvec2(srcFrame - dstFrame) / dstFrame.z : bias;
            if (shape.color.a * source.opacity == 0.0f)
                continue;
            if (input.shapes.size() >= 4096u)
                return Failure(Failure::ConfigurationError, "Combined vector tile exceeds 4096 paint operations");
            SlugShapeInput out;
            out.owner = static_cast<std::uint32_t>(input.shapes.size());
            if (shape.coverageGroup == 0u || previousGroup == 0u || previousGroup != shape.coverageGroup)
                ++paintGroup;
            previousGroup = shape.coverageGroup;
            paintGroups.push_back(paintGroup);
            glm::vec4 projection(0);
            if (mercator && dst.srs() == SRS::WGS84)
                projection = glm::vec4((dst.ymin() + dst.ymax()) * 0.5 * pi / 180.0,
                    dst.height() * pi / 180.0, 180.0 / (dst.width() * pi), 1.0);
            else if (mercator)
                projection = glm::vec4(0, dst.height() / dst.width(), 0, 2);
            projections.push_back(projection);
            out.kind = shape.type == VectorTile::Shape::Type::Fill ? SlugShapeKind::Fill : SlugShapeKind::Stroke;
            out.color = { shape.color.r, shape.color.g, shape.color.b, shape.color.a * source.opacity };
            out.strokeWidth = static_cast<float>(shape.width * shapeScale.x);
            if (shape.gap > 0 && out.kind == SlugShapeKind::Stroke)
            {
                out.strokeWidth = static_cast<float>(shape.gap * shapeScale.x);
                out.outlineWidth = static_cast<float>(shape.width * shapeScale.x);
                out.outlineColor = out.color;
                out.color[3] = 0.0f;
            }
            const bool gpuStroke = out.kind == SlugShapeKind::Stroke && out.outlineWidth == 0.0f;
            strokeWidths.push_back(gpuStroke ? out.strokeWidth : 0.0f);
            // A bounded power-of-two envelope indexes every curve reachable by any width in this bucket.
            // Actual paint width stays in the SSBO; crossing the envelope rebuilds the acceleration bands.
            if (gpuStroke)
                out.strokeWidth = std::exp2(std::ceil(std::log2(out.strokeWidth)));
            glm::dvec2 lower(1e30), upper(-1e30);
            // Keep polygon/hole associations so the adapter can partition large batches safely.
            auto appendContours = [&](const std::vector<std::vector<glm::dvec2>>& contours)
            {
                SlugPolygonInput polygon;
                for (std::size_t c = 0; c < contours.size(); ++c)
                {
                    SlugContourInput contour;
                    contour.closed = out.kind == SlugShapeKind::Fill;
                    for (auto p : contours[c])
                    {
                        p = p * shapeScale + shapeBias;
                        lower = glm::min(lower, p);
                        upper = glm::max(upper, p);
                        contour.points.push_back({ static_cast<float>(p.x), static_cast<float>(p.y) });
                    }
                    if (contour.closed)
                    {
                        double area = 0.0;
                        for (std::size_t i = 0, j = contour.points.size() - 1; i < contour.points.size(); j = i++)
                            area += double(contour.points[j].x) * contour.points[i].y -
                                double(contour.points[i].x) * contour.points[j].y;
                        if ((area < 0.0) != (c != 0u))
                            std::reverse(contour.points.begin(), contour.points.end());
                        if (c == 0u) polygon.outer = std::move(contour);
                        else polygon.holes.push_back(std::move(contour));
                    }
                    else out.contours.push_back(std::move(contour));
                }
                if (out.kind == SlugShapeKind::Fill)
                    out.polygons.push_back(std::move(polygon));
            };
            if (!shape.contours.empty()) appendContours(shape.contours);
            for (const auto& polygon : shape.polygons) appendContours(polygon);
            // Small islands/road fragments far from UV zero lose atlas precision unless
            // authored near their own origin. Keep the carrier UV mapping independent.
            const auto origin = glm::vec2((lower + upper) * 0.5);
            auto recenter = [&](SlugContourInput& contour) {
                for (auto& p : contour.points) { p.x -= origin.x; p.y -= origin.y; }
            };
            for (auto& contour : out.contours) recenter(contour);
            for (auto& polygon : out.polygons)
            {
                recenter(polygon.outer);
                for (auto& hole : polygon.holes) recenter(hole);
            }
            out.uvToEmX[2] = -origin.x;
            out.uvToEmY[2] = -origin.y;
            const double padding = (out.kind == SlugShapeKind::Stroke ?
                out.strokeWidth * 0.5 + out.outlineWidth : 0.0);
            lower -= padding;
            upper += padding;
            if (mercator)
            {
                // Convert padded metric bounds back to the carrier UV grid conservatively.
                const auto carrierY = [&](double y) {
                    const double northing = y * dstFrame.z + dstFrame.y;
                    const double ordinate = dst.srs() == SRS::WGS84 ?
                        std::atan(std::sinh(northing / radius)) * 180.0 / pi : northing;
                    return (ordinate - dst.ymin()) / dst.height();
                };
                lower.y = carrierY(lower.y);
                upper.y = carrierY(upper.y);
            }
            lower -= 1.0 / 16.0;
            upper += 1.0 / 16.0;
            bounds.emplace_back(lower.x, lower.y, upper.x, upper.y);
            input.shapes.push_back(std::move(out));
        }
    }

    // Exact serialized geometry keys avoid hash-collision aliasing. Expired entries are pruned on publication;
    // the cache itself owns no atlas resources. Concurrent builders may harmlessly publish equivalent atlases.
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::weak_ptr<const TerrainVectorGeometry>> cache;
    std::string identity;
    const auto keyValue = [&](const auto& value) {
        identity.append(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    const auto keyContour = [&](const SlugContourInput& contour) {
        keyValue(contour.closed);
        keyValue(contour.points.size());
        for (const auto& point : contour.points) { keyValue(point.x); keyValue(point.y); }
    };
    for (const auto& shape : input.shapes)
    {
        keyValue(shape.kind);
        keyValue(shape.uvToEmX);
        keyValue(shape.uvToEmY);
        keyValue(shape.strokeWidth);
        keyValue(shape.outlineWidth);
        keyValue(shape.contours.size());
        for (const auto& contour : shape.contours) keyContour(contour);
        keyValue(shape.polygons.size());
        for (const auto& polygon : shape.polygons)
        {
            keyContour(polygon.outer);
            keyValue(polygon.holes.size());
            for (const auto& hole : polygon.holes) keyContour(hole);
        }
        // Baked outline paints can affect which layers the adapter emits; keep them in the key.
        if (shape.outlineWidth > 0.0f) { keyValue(shape.color); keyValue(shape.outlineColor); }
    }
    std::shared_ptr<const TerrainVectorGeometry> geometry;
    {
        std::scoped_lock lock(cacheMutex);
        const auto found = cache.find(identity);
        if (found != cache.end()) geometry = found->second.lock();
    }
    const bool reused = bool(geometry);
    SlugAtlasOutput atlas;
    if (!geometry && !input.shapes.empty())
    {
        std::string error;
        if (!buildSlugAtlas(input, atlas, error) || !checkSlugAtlasDimensions(atlas, maxImageDimension, error))
            return Failure(error);
    }
    if (io.canceled())
        return Failure_OperationCanceled;

    auto result = TerrainVectorTexture::create();
    if (atlas.layers.size() > 4096u)
        return Failure("Terrain vector atlas exceeds 4096 layers");
    result->key = request.key;
    result->signature = request.signature;
    if (!geometry)
    {
        auto created = std::make_shared<TerrainVectorGeometry>();
        auto sampler = vsg::Sampler::create();
        sampler->minFilter = sampler->magFilter = VK_FILTER_NEAREST;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler->addressModeU = sampler->addressModeV = sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->maxLod = 0.0f;
        auto curves = vsg::vec4Array2D::create(std::max(1u, atlas.curveTexture.width),
            std::max(1u, atlas.curveTexture.height), vsg::Data::Properties{ VK_FORMAT_R32G32B32A32_SFLOAT });
        auto bands = vsg::usvec2Array2D::create(std::max(1u, atlas.bandTexture.width),
            std::max(1u, atlas.bandTexture.height), vsg::Data::Properties{ VK_FORMAT_R16G16_UINT });
        std::memset(curves->dataPointer(), 0, curves->dataSize());
        std::memset(bands->dataPointer(), 0, bands->dataSize());
        if (!atlas.layers.empty())
        {
            if (atlas.curveTexture.format != SlugTextureFormat::RGBA32F ||
                atlas.bandTexture.format != SlugTextureFormat::RG16UI ||
                atlas.curveTexture.bytes.size() != curves->dataSize() || atlas.bandTexture.bytes.size() != bands->dataSize())
                return Failure("Malformed terrain vector atlas");
            std::memcpy(curves->dataPointer(), atlas.curveTexture.bytes.data(), curves->dataSize());
            std::memcpy(bands->dataPointer(), atlas.bandTexture.bytes.data(), bands->dataSize());
        }
        created->curves = vsg::DescriptorImage::create(sampler, curves, BINDING_TERRAIN_VECTOR_CURVES);
        created->bands = vsg::DescriptorImage::create(sampler, bands, BINDING_TERRAIN_VECTOR_BANDS);
        created->textureWidthLog2 = atlas.textureWidthLog2;
        created->maxDimension = std::max({curves->width(), curves->height(), bands->width(), bands->height()});
        created->layers = std::move(atlas.layers);
        // Terrain doesn't perform CPU coverage picking; retain only GPU atlas data and placement records.
        for (auto& layer : created->layers) layer.coverage.reset();
        geometry = created;
        {
            std::scoped_lock lock(cacheMutex);
            for (auto i = cache.begin(); i != cache.end();)
                if (i->second.expired()) i = cache.erase(i); else ++i;
            cache[std::move(identity)] = geometry;
        }
    }
    if (maxImageDimension != 0 && geometry->maxDimension > maxImageDimension)
        return Failure("Cached terrain vector atlas exceeds device image dimension limit");
    result->geometry = geometry;
    result->reusedGeometry = reused;
    result->curves = geometry->curves;
    result->bands = geometry->bands;
    result->layerCount = static_cast<std::uint32_t>(geometry->layers.size());

    // Header is a std430 uvec4, followed by 96-byte terrain paint records.
    auto records = vsg::ubyteArray::create(16u + std::max(1u, result->layerCount) * sizeof(TerrainVectorLayerGPU));
    std::memset(records->dataPointer(), 0, records->dataSize());
    const glm::uvec4 header(result->layerCount, geometry->textureWidthLog2, 16u, 0u);
    std::memcpy(records->dataPointer(), &header, sizeof(header));
    std::vector<std::vector<std::uint32_t>> cells(256u);
    for (std::uint32_t i = 0; i < result->layerCount; ++i)
    {
        const auto& layer = geometry->layers[i];
        TerrainVectorLayerGPU record;
        std::memcpy(&record.color, layer.color.data(), 16);
        if (input.shapes.at(layer.owner).outlineWidth == 0.0f)
            std::memcpy(&record.color, input.shapes.at(layer.owner).color.data(), 16);
        std::memcpy(&record.uvToEmX, layer.uvToEmX.data(), 16);
        if (record.uvToEmX.w > 0.0f)
            record.uvToEmX.w = strokeWidths.at(layer.owner) * 0.5f;
        std::memcpy(&record.uvToEmY, layer.uvToEmY.data(), 16);
        record.uvToEmY.w = static_cast<float>(paintGroups.at(layer.owner));
        record.projection = projections.at(layer.owner);
        std::memcpy(&record.bandTransform, layer.bandTransform.data(), 16);
        std::memcpy(&record.shapeData, layer.shapeData.data(), 16);
        std::memcpy(static_cast<std::uint8_t*>(records->dataPointer()) + 16u + i * sizeof(record), &record, sizeof(record));
        const auto b = bounds.at(layer.owner);
        for (unsigned y = 0; y < 16u; ++y)
            for (unsigned x = 0; x < 16u; ++x)
                if (b.x <= (x + 1) / 16.0f && b.z >= x / 16.0f && b.y <= (y + 1) / 16.0f && b.w >= y / 16.0f)
                    cells[y * 16u + x].push_back(i);
    }
    result->layers = vsg::DescriptorBuffer::create(records, BINDING_TERRAIN_VECTOR_LAYERS, 0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    std::vector<std::uint32_t> words(512u, 0u);
    for (unsigned c = 0; c < 256u; ++c)
    {
        words[c * 2u] = static_cast<std::uint32_t>(words.size());
        words[c * 2u + 1u] = static_cast<std::uint32_t>(cells[c].size());
        words.insert(words.end(), cells[c].begin(), cells[c].end());
    }
    auto grid = vsg::uintArray::create(words.size());
    std::memcpy(grid->dataPointer(), words.data(), words.size() * sizeof(std::uint32_t));
    result->grid = vsg::DescriptorBuffer::create(grid, BINDING_TERRAIN_VECTOR_GRID, 0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    return result;
#endif
}

std::optional<double> rocky::terrainViewStyleZoom(const SRS& worldSRS,
    const glm::dmat4& modelview, const glm::dmat4& projection, double viewportHeight)
{
    if (!(viewportHeight > 0.0) || !(std::abs(projection[1][1]) > 0.0)) return {};
    const auto inverse = glm::inverse(modelview);
    const glm::dvec3 eye(inverse * glm::dvec4(0, 0, 0, 1));
    const auto direction = glm::normalize(glm::dvec3(inverse * glm::dvec4(0, 0, -1, 0)));
    double distance = 0.0, mercatorScale = 1.0;
    if (worldSRS.isGeocentric())
    {
        const auto& ellipsoid = worldSRS.ellipsoid();
        const glm::dvec3 axes(ellipsoid.semiMajorAxis(), ellipsoid.semiMajorAxis(), ellipsoid.semiMinorAxis());
        const auto p = eye / axes, d = direction / axes;
        const double a = glm::dot(d, d), b = glm::dot(p, d), c = glm::dot(p, p) - 1.0;
        const double discriminant = b*b - a*c;
        if (discriminant < 0.0 || !(a > 0.0)) return {};
        // Stable near root avoids subtracting almost equal numbers close to the surface.
        const double farRoot = (-b + std::sqrt(discriminant)) / a;
        if (!(farRoot > 0.0)) return {};
        distance = c / (a * farRoot);
        if (!(distance > 0.0)) return {};
        const auto focal = GeoPoint(worldSRS, eye + direction * distance).transform(SRS::WGS84);
        if (!focal.valid()) return {};
        mercatorScale = std::max(std::cos(focal.y * pi / 180.0), 1e-6);
    }
    else if (worldSRS.isProjected())
    {
        if (std::abs(direction.z) < 1e-12) return {};
        distance = -eye.z / direction.z;
        if (!(distance > 0.0)) return {};
        if (worldSRS != SRS::SPHERICAL_MERCATOR)
        {
            // Estimate local Mercator scale in projected map units, retaining arbitrary map units.
            auto transform = worldSRS.to(SRS::SPHERICAL_MERCATOR);
            const auto focal = eye + direction*distance;
            const double step = std::max(1e-3, std::max(std::abs(focal.x), std::abs(focal.y))*1e-7);
            glm::dvec3 center, east, north;
            if (!transform.transform(focal, center) ||
                !transform.transform(focal+glm::dvec3(step,0,0), east) ||
                !transform.transform(focal+glm::dvec3(0,step,0), north)) return {};
            // Wrapped longitudes must not turn a small local offset into a world-wide distance.
            const double circumference = 2.0*pi*radius;
            east.x = center.x + std::remainder(east.x-center.x, circumference);
            north.x = center.x + std::remainder(north.x-center.x, circumference);
            const double scale = std::sqrt(glm::length(glm::dvec2(east-center))*
                glm::length(glm::dvec2(north-center))) / step;
            if (!(scale > 0.0) || !std::isfinite(scale)) return {};
            mercatorScale = 1.0/scale;
        }
    }
    else return {};
    const bool orthographic = projection[3][3] != 0.0;
    const double metersPerPixel = 2.0 * (orthographic ? 1.0 : distance) /
        (std::abs(projection[1][1]) * viewportHeight);
    const double zoom = std::log2(2.0 * pi * radius * mercatorScale / (512.0 * metersPerPixel));
    return std::isfinite(zoom) ? std::optional<double>(std::clamp(zoom, 0.0, 22.0)) : std::nullopt;
}
