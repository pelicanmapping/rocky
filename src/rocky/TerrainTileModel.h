/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/TileKey.h>
#include <rocky/Math.h>
#include <rocky/GeoImage.h>
#include <rocky/GeoHeightfield.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    class Image;
    class Heightfield;
    class Layer;
    class Map;

    /**
     * Set of layers that the TerrainTileModelFactory can use
     * to determine what data to create for a tile. If the manifest
     * is empty, that represents that the factory should create
     * data for ALL available layers. But if it contains at least
     * one entry, the creation is limited to those entries.
     */
    class ROCKY_EXPORT CreateTileManifest
    {
    public:
        CreateTileManifest();

        //! Request data for a layer
        void insert(std::shared_ptr<Layer> layer);

        //! Sets whether to apply the update progressively (in LOD order)
        void setProgressive(bool value);
        const optional<bool>& progressive() const { return _progressive; }

        //! Is the manifest empty (meaning ALL available layers should be loaded)?
        bool empty() const;

        //! Are the layers in the manifest up to date with the layers in the map?
        bool inSyncWith(const Map*) const;

        //! Update all the manifest layers with the latest layer revisions from the map
        void updateRevisions(const Map*);

        bool includes(UID uid) const;

        bool includes(const Layer* layer) const;

        bool includesElevation() const;

        bool includesConstraints() const;

    private:
        using LayerTable = std::unordered_map<UID, Revision>;
        LayerTable _layers;
        bool _includesElevation;
        bool _includesConstraints;
        optional<bool> _progressive;
    };

    /**
     * Data model backing an individual terrain tile.
     */
    class ROCKY_EXPORT TerrainTileModel
    {
    public:
        struct ROCKY_EXPORT Tile
        {
            TileKey key;
            Revision revision = -1;
            glm::fmat4 matrix = glm::fmat4(1.0f); // identity
        };

        struct ROCKY_EXPORT ColorLayer : public Tile
        {
            GeoImage image;
            std::shared_ptr<const Layer> layer;
            using Vector = std::vector<ColorLayer>;
        };

        struct ROCKY_EXPORT Elevation : public Tile
        {
            float minHeight = FLT_MAX;
            float maxHeight = -FLT_MAX;
            GeoHeightfield heightfield;
        };

        struct ROCKY_EXPORT NormalMap : public Tile
        {
            GeoImage image;
            std::shared_ptr<const Layer> layer;
        };

        struct ROCKY_EXPORT MaterialMap : public Tile
        {
        };

        //! Map model revision from which this model was created
        Revision revision = -1;

        //! Tile key corresponding to this model
        TileKey key;

        //! Whether some data here requires updates
        bool requiresUpdate = false;

        //! Imagery and other surface coloring layers
        ColorLayer::Vector colorLayers;

        //! Elevation data
        Elevation elevation;

        //! Normal map data
        NormalMap normalMap;

        //! Material map data
        MaterialMap materialMap;

        ////! Get a texture given a layer ID
        //GeoImage getColorLayerImage(UID layerUID) const;

        ////! Get a matrix given a layer ID
        //const fmat4& getColorLayerMatrix(UID layerUID) const;
    };

}
