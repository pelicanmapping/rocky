/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/MBTiles.h>
#ifdef ROCKY_HAS_MBTILES

#include <rocky/ElevationLayer.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Elevation layer reading from an MBTiles database.
     * GDAL support MBTiles directly, but this is an extended implementation
     * that supports SRS other than spherical mercator, customizable image
     * formats, and tile compression.
     */
    class ROCKY_EXPORT MBTilesElevationLayer : public Inherit<ElevationLayer, MBTilesElevationLayer>
    {
    public:
        //! Construct an empty layer
        MBTilesElevationLayer();
        explicit MBTilesElevationLayer(const JSON&);

        //! Location of the mbtiles database file
        void setURI(const URI& value) { _options.uri = value; }
        optional<URI>& uri() { return _options.uri; }

        //! Content type of the individual tiles in the database (e.g., image/tif)
        void setFormat(const std::string& value) { _options.format = value; }
        optional<std::string>& format() { return _options.format; }

        //! Whether to use compression on individual tile data
        void setCompress(bool value) { _options.compress = value; }
        optional<bool>& compress() { return _options.compress; }

        //! serialize
        JSON to_json() const override;

    protected:

        //! Creates a raster image for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const override;

        //! Opens the layer and returns its status
        Status openImplementation(const IOOptions& io) override;

        //! Closes the layer and returns its status
        void closeImplementation() override;

        //! Writes a raster image for he given tile key (if open for writing)
        Status writeHeightfieldImplementation(const TileKey& key, shared_ptr<Heightfield> image, const IOOptions& io) const override;


    private:
        MBTiles::Driver _driver;
        MBTiles::Options _options;
        void construct(const JSON&);
    };
}

#endif // ROCKY_HAS_MBTILES
