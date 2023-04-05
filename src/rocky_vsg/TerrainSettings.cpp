/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainSettings.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

TerrainSettings::TerrainSettings(const JSON& conf)
{
    const auto j = parse_json(conf);

    get_to(j, "tile_size", tileSize);
    get_to(j, "min_tile_range_factor", minTileRangeFactor);
    get_to(j, "max_level", maxLevelOfDetail);
    get_to(j, "min_level", minLevelOfDetail);
    get_to(j, "tessellation", gpuTessellation);
    get_to(j, "tessellation_level", tessellationLevel);
    get_to(j, "tessellation_range", tessellationRange);
    get_to(j, "min_seconds_before_unload", minSecondsBeforeUnload);
    get_to(j, "min_frames_before_unload", minFramesBeforeUnload);
    get_to(j, "min_tiles_before_unload", minResidentTilesBeforeUnload);
    get_to(j, "cast_shadows", castShadows);
    get_to(j, "tile_pixel_size", tilePixelSize);
    get_to(j, "skirt_ratio", skirtRatio);
    get_to(j, "color", color);
    get_to(j, "normalize_edges", normalizeEdges);
    get_to(j, "morph_terrain", morphTerrain);
    get_to(j, "morph_imagery", morphImagery);
    get_to(j, "concurrency", concurrency);
    get_to(j, "wireframe_overlay", wireframeOverlay);
}

JSON
TerrainSettings::to_json() const
{
    auto j = json::object();
    set(j, "tile_size", tileSize);
    set(j, "min_tile_range_factor", minTileRangeFactor);
    set(j, "max_level", maxLevelOfDetail);
    set(j, "min_level", minLevelOfDetail);
    set(j, "tessellation", gpuTessellation);
    set(j, "tessellation_level", tessellationLevel);
    set(j, "tessellation_range", tessellationRange);
    set(j, "min_seconds_before_unload", minSecondsBeforeUnload);
    set(j, "min_frames_before_unload", minFramesBeforeUnload);
    set(j, "min_tiles_before_unload", minResidentTilesBeforeUnload);
    set(j, "cast_shadows", castShadows);
    set(j, "tile_pixel_size", tilePixelSize);
    set(j, "skirt_ratio", skirtRatio);
    set(j, "color", color);
    set(j, "normalize_edges", normalizeEdges);
    set(j, "morph_terrain", morphTerrain);
    set(j, "morph_imagery", morphImagery);
    set(j, "concurrency", concurrency);
    set(j, "wireframe_overlay", wireframeOverlay);
    return j.dump();
}
