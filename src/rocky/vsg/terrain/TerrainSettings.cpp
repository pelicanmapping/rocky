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

    get_to(j, "tileSize", tileSize);
    get_to(j, "minTileRangeFactor", minTileRangeFactor);
    get_to(j, "maxLevel", maxLevel);
    get_to(j, "minLevel", minLevel);
    get_to(j, "pixelError", pixelError);
    get_to(j, "tilePixelSize", tilePixelSize);
    get_to(j, "skirtRatio", skirtRatio);
    get_to(j, "color", color);
    get_to(j, "concurrency", concurrency);
    get_to(j, "wireOverlay", wireOverlay);

    return ResultVoidOK;
}

std::string
TerrainSettings::to_json() const
{
    auto j = json::object();
    set(j, "tileSize", tileSize);
    set(j, "minTileRangeFactor", minTileRangeFactor);
    set(j, "maxLevel", maxLevel);
    set(j, "minLevel", minLevel);
    set(j, "pixelError", pixelError);
    set(j, "tilePixelSize", tilePixelSize);
    set(j, "skirtRatio", skirtRatio);
    set(j, "color", color);
    set(j, "concurrency", concurrency);
    set(j, "wireOverlay", wireOverlay);
    return j.dump();
}
