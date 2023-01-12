/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainOptions.h"
#include <cstdlib> // for getenv

using namespace ROCKY_NAMESPACE;

#undef LC
#define LC "[TerrainOptions] "

//...................................................................

Config
TerrainOptions::getConfig() const
{
    Config conf = DriverConfigOptions::getConfig();
    conf.key() = "terrain";

    conf.set("tile_size", _tileSize);
    conf.set("min_tile_range_factor", _minTileRangeFactor);
    conf.set("range_factor", _minTileRangeFactor);
    conf.set("max_lod", _maxLOD);
    conf.set("min_lod", _minLOD);
    conf.set("first_lod", _firstLOD);
    conf.set("tessellation_level", tessellationLevel());
    conf.set("tessellation_range", tessellationRange());
    conf.set("min_expiry_time", _minExpiryTime);
    conf.set("min_expiry_frames", _minExpiryFrames);
    conf.set("min_resident_tiles", minResidentTiles());
    conf.set("cast_shadows", _castShadows);
    conf.set("tile_pixel_size", _tilePixelSize);
    conf.set("skirt_ratio", heightfieldSkirtRatio());
    conf.set("color", color());
    conf.set("morph_terrain", morphTerrain());
    conf.set("morph_elevation", morphTerrain());
    conf.set("morph_imagery", morphImagery());
    conf.set("concurrency", concurrency());
    //conf.set("screen_space_error", screenSpaceError()); // don't serialize me, i'm set by the MapNode

    return conf;
}

void
TerrainOptions::fromConfig(const Config& conf)
{
    tileSize().setDefault(17);
    minTileRangeFactor().setDefault(7.0);
    maxLOD().setDefault(19u);
    minLOD().setDefault(0u);
    firstLOD().setDefault(0u);
    tessellationLevel().setDefault(2.5f);
    tessellationRange().setDefault(75.0f);
    castShadows().setDefault(false);
    tilePixelSize().setDefault(256);
    minExpiryFrames().setDefault(0);
    minExpiryTime().setDefault(0.0);
    minExpiryRange().setDefault(0.0f);
    minResidentTiles().setDefault(0u);
    heightfieldSkirtRatio().setDefault(0.0f);
    color().setDefault(Color::White);
    morphTerrain().setDefault(true);
    morphImagery().setDefault(true);
    concurrency().setDefault(4u);
    screenSpaceError().setDefault(0.0f);

    conf.get("tile_size", _tileSize);
    conf.get("min_tile_range_factor", _minTileRangeFactor);
    conf.get("range_factor", _minTileRangeFactor);
    conf.get("max_lod", _maxLOD); conf.get("max_level", _maxLOD);
    conf.get("min_lod", _minLOD); conf.get("min_level", _minLOD);
    conf.get("first_lod", _firstLOD); conf.get("first_level", _firstLOD);
    conf.get("tessellation_level", tessellationLevel());
    conf.get("tessellation_range", tessellationRange());
    conf.get("min_expiry_time", _minExpiryTime);
    conf.get("min_expiry_frames", _minExpiryFrames);
    conf.get("min_resident_tiles", minResidentTiles());
    conf.get("cast_shadows", _castShadows);
    conf.get("tile_pixel_size", _tilePixelSize);
    conf.get("skirt_ratio", heightfieldSkirtRatio());
    conf.get("color", color());
    conf.get("morph_terrain", morphTerrain());
    conf.get("morph_imagery", morphImagery());
    conf.get("concurrency", concurrency());
    //conf.get("screen_space_error", screenSpaceError()); // don't serialize me, i'm set by the MapNode

    conf.get("expiration_range", minExpiryRange()); // legacy
    conf.get("expiration_threshold", minResidentTiles()); // legacy

    // report on deprecated usage
    const std::string deprecated_keys[] = {
        "compress_normal_maps",
        "min_expiry_frames",
        "expiration_threshold",
        "priority_scale"
    };
    for (const auto& key : deprecated_keys)
    {
        if (conf.hasValue(key))
        {
            //ROCKY_INFO << LC << "Deprecated key \"" << key << "\" ignored" << std::endl;
        }
    }
}

//...................................................................

TerrainOptionsAPI::TerrainOptionsAPI(TerrainOptions* optionsPtr) :
    _ptr(optionsPtr)
{
    //nop
}

ROCKY_OPTION_IMPL(TerrainOptionsAPI, int, TileSize, tileSize);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, MinTileRangeFactor, minTileRangeFactor);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, MaxLOD, maxLOD);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, MinLOD, minLOD);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, FirstLOD, firstLOD);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, TessellationLevel, tessellationLevel);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, TessellationRange, tessellationRange);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, bool, CastShadows, castShadows);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, TilePixelSize, tilePixelSize);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, MinExpiryFrames, minExpiryFrames);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, double, MinExpiryTime, minExpiryTime);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, MinExpiryRange, minExpiryRange);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, MinResidentTiles, minResidentTiles);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, HeightfieldSkirtRatio, heightfieldSkirtRatio);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, Color, Color, color);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, bool, MorphTerrain, morphTerrain);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, bool, MorphImagery, morphImagery);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, unsigned, Concurrency, concurrency);
ROCKY_OPTION_IMPL(TerrainOptionsAPI, float, ScreenSpaceError, screenSpaceError);
