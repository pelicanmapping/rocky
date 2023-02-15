/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainSettings.h"

using namespace ROCKY_NAMESPACE;

TerrainSettings::TerrainSettings(const Config& conf)
{
    conf.get("tile_size", tileSize);
    conf.get("min_tile_range_factor", minTileRangeFactor);
    conf.get("max_lod", maxLevelOfDetail); conf.get("max_level", maxLevelOfDetail);
    conf.get("min_lod", minLevelOfDetail); conf.get("min_level", minLevelOfDetail);
    conf.get("tessellation", gpuTessellation);
    conf.get("tessellation_level", tessellationLevel);
    conf.get("tessellation_range", tessellationRange);
    conf.get("min_seconds_before_unload", minSecondsBeforeUnload);
    conf.get("min_frames_before_unload", minFramesBeforeUnload);
    conf.get("min_tiles_before_unload", minResidentTilesBeforeUnload);
    conf.get("cast_shadows", castShadows);
    conf.get("tile_pixel_size", tilePixelSize);
    conf.get("skirt_ratio", skirtRatio);
    conf.get("color", color);
    conf.get("normalize_edges", normalizeEdges);
    conf.get("morph_terrain", morphTerrain);
    conf.get("morph_imagery", morphImagery);
    conf.get("concurrency", concurrency);
}

void
TerrainSettings::saveToConfig(Config& conf) const
{
    conf.set("tile_size", tileSize);
    conf.set("min_tile_range_factor", minTileRangeFactor);
    conf.set("max_lod", maxLevelOfDetail); conf.set("max_level", maxLevelOfDetail);
    conf.set("min_lod", minLevelOfDetail); conf.set("min_level", minLevelOfDetail);
    conf.set("tessellation", gpuTessellation);
    conf.set("tessellation_level", tessellationLevel);
    conf.set("tessellation_range", tessellationRange);
    conf.set("min_seconds_before_unload", minSecondsBeforeUnload);
    conf.set("min_frames_before_unload", minFramesBeforeUnload);
    conf.set("min_tiles_before_unload", minResidentTilesBeforeUnload);
    conf.set("cast_shadows", castShadows);
    conf.set("tile_pixel_size", tilePixelSize);
    conf.set("skirt_ratio", skirtRatio);
    conf.set("color", color);
    conf.set("normalize_edges", normalizeEdges);
    conf.set("morph_terrain", morphTerrain);
    conf.set("morph_imagery", morphImagery);
    conf.set("concurrency", concurrency);
}
