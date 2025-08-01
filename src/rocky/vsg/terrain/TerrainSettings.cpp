/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainSettings.h"
#include "../json.h"

using namespace ROCKY_NAMESPACE;

Result<>
TerrainSettings::from_json(std::string_view JSON)
{
    const auto j = parse_json(JSON);

    if (j.status.failed())
        return j.status.error();

    get_to(j, "tile_size", tileSize);
    get_to(j, "min_tile_range_factor", minTileRangeFactor);
    get_to(j, "max_level", maxLevelOfDetail);
    get_to(j, "min_level", minLevelOfDetail);
    get_to(j, "screen_space_error", screenSpaceError);
    get_to(j, "tile_pixel_size", tilePixelSize);
    get_to(j, "skirt_ratio", skirtRatio);
    get_to(j, "color", color);
    get_to(j, "concurrency", concurrency);

    return {};
}

std::string
TerrainSettings::to_json() const
{
    auto j = json::object();
    set(j, "tile_size", tileSize);
    set(j, "min_tile_range_factor", minTileRangeFactor);
    set(j, "max_level", maxLevelOfDetail);
    set(j, "min_level", minLevelOfDetail);
    set(j, "screen_space_error", screenSpaceError);
    set(j, "tile_pixel_size", tilePixelSize);
    set(j, "skirt_ratio", skirtRatio);
    set(j, "color", color);
    set(j, "concurrency", concurrency);
    return j.dump();
}
