/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/MBTiles.h>
#ifdef ROCKY_HAS_MBTILES

#include <rocky/ImageLayer.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Image layer reading from an MBTiles database.
     * GDAL support MBTiles directly, but this is an extended implementation
     * that supports SRS other than spherical mercator, customizable image
     * formats, and tile compression.
     */
    class ROCKY_EXPORT MBTilesImageLayer : public Inherit<ImageLayer, MBTilesImageLayer>,
        public MBTiles::Options
    {
    public:
        //! Construct an empty layer
        MBTilesImageLayer();
        explicit MBTilesImageLayer(std::string_view JSON, const IOOptions& io);

        //! serialize
        JSON to_json() const override;

    protected:

        //! Opens the layer and returns its status
        Status openImplementation(const IOOptions& io) override;

        //! Closes the layer and returns its status
        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

    private:
        MBTiles::Driver _driver;

        void construct(std::string_view JSON, const IOOptions& io);
    };
}

#endif // ROCKY_HAS_MBTILES
