/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "VectorTileLayer.h"
#include <cmath>

using namespace ROCKY_NAMESPACE;

Result<> VectorTile::validate() const
{
    if (shapes.size() > 4096u)
        return Failure(Failure::ConfigurationError, "Vector tile exceeds 4096 paint operations");
    std::size_t points = 0u;
    const Shape* previous = nullptr;
    for (const auto& shape : shapes)
    {
        if (shape.type != Shape::Type::Fill && shape.type != Shape::Type::Line)
            return Failure(Failure::ConfigurationError, "Invalid vector primitive");
        if (shape.coordinates != Shape::Coordinates::TileUV && shape.coordinates != Shape::Coordinates::Mercator)
            return Failure(Failure::ConfigurationError, "Invalid vector coordinate space");
        for (unsigned c = 0; c < 4; ++c)
            if (!std::isfinite(shape.color[c]) || shape.color[c] < 0.0f || shape.color[c] > 1.0f)
                return Failure(Failure::ConfigurationError, "Vector color must be finite RGBA in [0,1]");
        if (shape.type == Shape::Type::Line && (!std::isfinite(shape.width) || shape.width <= 0.0f))
            return Failure(Failure::ConfigurationError, "Vector line width must be finite and positive");
        if (!std::isfinite(shape.gap) || shape.gap < 0.0f)
            return Failure(Failure::ConfigurationError, "Vector line gap must be finite and nonnegative");
        if (shape.coverageGroup != 0u && (shape.gap > 0.0f ||
            (previous && previous->coverageGroup == shape.coverageGroup &&
                (previous->color != shape.color || previous->type != shape.type))))
            return Failure(Failure::ConfigurationError, "Grouped coverage requires equal-colored compatible primitives");
        previous = &shape;
        if (shape.contours.empty() && shape.polygons.empty())
            return Failure(Failure::ConfigurationError, "Vector shape has no contours");
        if (shape.type == Shape::Type::Line && !shape.polygons.empty())
            return Failure(Failure::ConfigurationError, "Line shapes cannot contain polygons");
        std::vector<const std::vector<std::vector<glm::dvec2>>*> groups;
        if (!shape.contours.empty()) groups.push_back(&shape.contours);
        for (const auto& polygon : shape.polygons)
        {
            if (polygon.empty()) return Failure(Failure::ConfigurationError, "Empty vector polygon");
            groups.push_back(&polygon);
        }
        for (const auto* group : groups)
        for (const auto& contour : *group)
        {
            if (contour.size() < (shape.type == Shape::Type::Fill ? 3u : 2u))
                return Failure(Failure::ConfigurationError, "Vector contour has too few points");
            points += contour.size();
            if (points > 1000000u)
                return Failure(Failure::ConfigurationError, "Vector tile exceeds one million points");
            for (const auto& p : contour)
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 1024.0 || std::abs(p.y) > 1024.0)
                    return Failure(Failure::ConfigurationError, "Vector coordinates must be finite and localized");
        }
    }
    return ResultVoidOK;
}

VectorTileLayer::VectorTileLayer()
{
    setLayerTypeName("VectorTile");
}

VectorTileLayer::VectorTileLayer(std::string_view JSON, const IOOptions&) :
    super(JSON)
{
    setLayerTypeName("VectorTile");
}

Result<> VectorTileLayer::openImplementation(const IOOptions& io)
{
#ifndef ROCKY_HAS_SLUGHORN
    return Failure(Failure::ServiceUnavailable, "Terrain vector textures require Slughorn support");
#else
    return super::openImplementation(io);
#endif
}

TileKey VectorTileLayer::bestAvailableTileKey(const TileKey& key) const
{
    if (!key.valid() || key.level < minLevel.value() || key.level > maxLevel.value())
        return {};
    return key.level > maxDataLevel.value() ? key.createAncestorKey(maxDataLevel.value()) : key;
}

Result<VectorTile> VectorTileLayer::createTile(
    const TileKey& key, const IOOptions& io, std::optional<double> styleZoom) const
{
    std::shared_lock lock(layerStateMutex());
    if (!isOpen())
        return Failure(Failure::ResourceUnavailable, "Vector layer is closed");
    if (!key.valid())
        return Failure(Failure::ConfigurationError, "Vector tile requires a valid destination key");
    if (io.canceled())
        return Failure_OperationCanceled;
    if (styleZoom && !std::isfinite(*styleZoom))
        return Failure(Failure::ConfigurationError, "Vector style zoom must be finite");
    auto result = createTileImplementation(key, io, styleZoom);
    if (result.failed())
        return result;
    auto valid = result.value().validate();
    if (valid.failed())
        return valid.error();
    return io.canceled() ? Result<VectorTile>(Failure_OperationCanceled) : std::move(result);
}

Result<VectorTile> VectorTileLayer::createTileImplementation(
    const TileKey& key, const IOOptions& io, std::optional<double>) const
{
    if (!createTileFunction)
        return Failure(Failure::ConfigurationError, "Vector layer has no tile producer");
    return createTileFunction(key, io);
}
