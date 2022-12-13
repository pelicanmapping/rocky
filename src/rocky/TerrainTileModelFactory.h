/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TerrainTileModel.h>
#include <rocky/TerrainOptions.h>
#include <unordered_map>

namespace rocky
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
        TerrainTileModelFactory();

        /**
         * Creates a tile model and populates it with data from the map.
         *
         * @param map          Map from which to read source data
         * @param key          Tile key for which to create the model
         * @param manifest     Set of layers for which to fetch data (empty => all layers)
         * @param progress     Progress tracking callback
         */
        virtual TerrainTileModel createTileModel(
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            const IOOptions& io);

        //! Same as createTileModel, except that this method will create "fallback"
        //! data for each layer that doesn't have real data at the TileKey's LOD.
        //! The texture matrix will each layer will provide a scale and bias for
        //! sampling the corresponding texture.
        virtual TerrainTileModel createStandaloneTileModel(
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            const IOOptions& io);

    protected:

        virtual void addColorLayers(
            TerrainTileModel& model,
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            const IOOptions& io,
            bool standalone);

        virtual bool addImageLayer(
            TerrainTileModel& model,
            shared_ptr<const ImageLayer> layer,
            const TileKey& key,
            const IOOptions& io);

        virtual void addStandaloneImageLayer(
            TerrainTileModel& model,
            shared_ptr<const ImageLayer> layer,
            const TileKey& key,
            const IOOptions& io);

        virtual bool addElevation(
            TerrainTileModel& model,
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            unsigned border,
            const IOOptions& io);

        virtual bool addStandaloneElevation(
            TerrainTileModel& model,
            const Map* map,
            const TileKey& key,
            const CreateTileManifest& manifest,
            unsigned border,
            const IOOptions& io);
    };
}
