/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/TileKey.h>
#include <rocky/GeoImage.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    class Image;
    class Heightfield;
    class Layer;
    class Map;

    /**
     * Data model backing an individual terrain tile.
     */
    struct TerrainTileModel
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
            GeoImage heightfield;
        };

        struct ROCKY_EXPORT NormalMap : public Tile
        {
            GeoImage image;
            std::shared_ptr<const Layer> layer;
        };

        struct ROCKY_EXPORT MaterialMap : public Tile
        {
        };

        //! True if this model contains no data.
        bool empty() const
        {
            return
                colorLayers.empty() &&
                !elevation.heightfield.valid();
        }

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
    };

}
