/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainSettings.h"

using namespace rocky;

TerrainSettings::TerrainSettings(const Config& conf)
{
    tileSize.setDefault(17);
    minTileRangeFactor.setDefault(4.0);
    maxLOD.setDefault(19u);
    minLOD.setDefault(0u);
    firstLOD.setDefault(0u);
    //enableLighting.setDefault(true);
    //clusterCulling().setDefault(true);
    //enableBlending().setDefault(true);
    //compressNormalMaps().setDefault(false);
    //minNormalMapLOD().setDefault(0);
    gpuTessellation.setDefault(false);
    tessellationLevel.setDefault(2.5f);
    tessellationRange.setDefault(75.0f);
    //debug.setDefault(false);
    //renderBinNumber.setDefault(0);
    castShadows.setDefault(false);
    //rangeMode().setDefault(osg::LOD::DISTANCE_FROM_EYE_POINT);
    tilePixelSize.setDefault(256);
    minFramesBeforeUnload.setDefault(0);
    minSecondsBeforeUnload.setDefault(0.0);
    minRangeBeforeUnload.setDefault(0.0f);
    minResidentTilesBeforeUnload.setDefault(0u);
    maxTilesToUnloadPerFrame.setDefault(~0u);
    skirtRatio.setDefault(0.0f);
    color.setDefault(Color::White);
    //progressive().setDefault(false);
    useNormalMaps.setDefault(true);
    normalizeEdges.setDefault(false);
    morphTerrain.setDefault(true);
    morphImagery.setDefault(true);
    //mergesPerFrame.setDefault(20u);
    //priorityScale().setDefault(1.0f);
    textureCompressionMethod.setDefault("");
    concurrency.setDefault(4u);
    //useLandCover.setDefault(true);
    screenSpaceError.setDefault(0.0f);


    conf.get("tile_size", tileSize);
    conf.get("min_tile_range_factor", minTileRangeFactor);
    conf.get("max_lod", maxLOD); conf.get("max_level", maxLOD);
    conf.get("min_lod", minLOD); conf.get("min_level", minLOD);
    conf.get("first_lod", firstLOD); conf.get("first_level", firstLOD);
    //conf.get("lighting", _enableLighting);
    //conf.get("cluster_culling", _clusterCulling);
    //conf.get("blending", _enableBlending);
    //conf.get("compress_normal_maps", _compressNormalMaps);
    //conf.get("min_normal_map_lod", _minNormalMapLOD);
    conf.get("tessellation", gpuTessellation);
    //conf.get("gpu_tessellation", _gpuTessellation); //bc
    conf.get("tessellation_level", tessellationLevel);
    conf.get("tessellation_range", tessellationRange);
    //conf.get("debug", _debug);
    //conf.get("bin_number", _renderBinNumber);
    conf.get("min_seconds_before_unload", minSecondsBeforeUnload);
    conf.get("min_frames_before_unload", minFramesBeforeUnload);
    conf.get("min_tiles_before_unload", minResidentTilesBeforeUnload);

    //conf.get("max_tiles_to_unload_per_frame", maxTilesToUnloadPerFrame);
    conf.get("cast_shadows", castShadows);
    conf.get("tile_pixel_size", tilePixelSize);
    //conf.get("range_mode", "PIXEL_SIZE_ON_SCREEN", rangeMode(), osg::LOD::PIXEL_SIZE_ON_SCREEN);
    //conf.get("range_mode", "pixel_size", rangeMode(), osg::LOD::PIXEL_SIZE_ON_SCREEN);
    //conf.get("range_mode", "DISTANCE_FROM_EYE_POINT", rangeMode(), osg::LOD::DISTANCE_FROM_EYE_POINT);
    //conf.get("range_mode", "distance", rangeMode(), osg::LOD::DISTANCE_FROM_EYE_POINT);
    conf.get("skirt_ratio", skirtRatio);
    conf.get("color", color);
    //conf.get("progressive", progressive());
    //conf.get("use_normal_maps", useNormalMaps());
    //conf.get("normal_maps", useNormalMaps()); // backwards compatible
    conf.get("normalize_edges", normalizeEdges);
    conf.get("morph_terrain", morphTerrain);
    conf.get("morph_imagery", morphImagery);
    //conf.get("merges_per_frame", mergesPerFrame());
    //conf.get("priority_scale", priorityScale());
    conf.get("texture_compression", textureCompressionMethod);
    conf.get("concurrency", concurrency);
    //conf.get("use_land_cover", useLandCover());
}

void
TerrainSettings::saveToConfig(Config& conf) const
{
    conf.set("tile_size", tileSize);
    conf.set("min_tile_range_factor", minTileRangeFactor);
    conf.set("max_lod", maxLOD); conf.set("max_level", maxLOD);
    conf.set("min_lod", minLOD); conf.set("min_level", minLOD);
    conf.set("first_lod", firstLOD); conf.set("first_level", firstLOD);
    //conf.get("lighting", _enableLighting);
    //conf.get("cluster_culling", _clusterCulling);
    //conf.get("blending", _enableBlending);
    //conf.get("compress_normal_maps", _compressNormalMaps);
    //conf.get("min_normal_map_lod", _minNormalMapLOD);
    conf.set("tessellation", gpuTessellation);
    //conf.get("gpu_tessellation", _gpuTessellation); //bc
    conf.set("tessellation_level", tessellationLevel);
    conf.set("tessellation_range", tessellationRange);
    //conf.get("debug", _debug);
    //conf.get("bin_number", _renderBinNumber);
    conf.set("min_seconds_before_unload", minSecondsBeforeUnload);
    conf.set("min_frames_before_unload", minFramesBeforeUnload);
    conf.set("min_tiles_before_unload", minResidentTilesBeforeUnload);

    //conf.get("max_tiles_to_unload_per_frame", maxTilesToUnloadPerFrame);
    conf.set("cast_shadows", castShadows);
    conf.set("tile_pixel_size", tilePixelSize);
    //conf.get("range_mode", "PIXEL_SIZE_ON_SCREEN", rangeMode(), osg::LOD::PIXEL_SIZE_ON_SCREEN);
    //conf.get("range_mode", "pixel_size", rangeMode(), osg::LOD::PIXEL_SIZE_ON_SCREEN);
    //conf.get("range_mode", "DISTANCE_FROM_EYE_POINT", rangeMode(), osg::LOD::DISTANCE_FROM_EYE_POINT);
    //conf.get("range_mode", "distance", rangeMode(), osg::LOD::DISTANCE_FROM_EYE_POINT);
    conf.set("skirt_ratio", skirtRatio);
    conf.set("color", color);
    //conf.get("progressive", progressive());
    //conf.get("use_normal_maps", useNormalMaps());
    //conf.get("normal_maps", useNormalMaps()); // backwards compatible
    conf.set("normalize_edges", normalizeEdges);
    conf.set("morph_terrain", morphTerrain);
    conf.set("morph_imagery", morphImagery);
    //conf.get("merges_per_frame", mergesPerFrame());
    //conf.get("priority_scale", priorityScale());
    conf.set("texture_compression", textureCompressionMethod);
    conf.set("concurrency", concurrency);
    //conf.get("use_land_cover", useLandCover());
}
