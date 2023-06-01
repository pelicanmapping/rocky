/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TerrainTileModel.h>
#include <unordered_map>

namespace ROCKY_NAMESPACE
{
    class ElevationLayerVector;
    class Map;
    class ImageLayer;
    class ElevationLayer;
    class IOControl;

    /**
     * Builds a TerrainTileModel from a map frame.
     */
    class ROCKY_EXPORT TerrainTileModelFactory
    {
    public:
        //! Whether to composite all color layers into one
        bool compositeColorLayers = true;

    public:
        TerrainTileModelFactory();

        //! Creates a tile model and populates it with data from the map.
        //! @param map Map from which to read source data
        //! @param key Tile key for which to create the model
        //! @param manifest Set of layers for which to fetch data (empty => all layers)
        //! @param io I/O options and cancelation callback
        TerrainTileModel createTileModel(
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            const IOOptions& io);

        TerrainTileModel::Elevation createElevationModel(
            const Map* map,
            const TileKey& key,
            const IOOptions& io) const;

    protected:

        void addColorLayers(
            TerrainTileModel& model,
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            const IOOptions& io,
            bool standalone);

        bool addElevation(
            TerrainTileModel& model,
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            unsigned border,
            const IOOptions& io);
    };
}
