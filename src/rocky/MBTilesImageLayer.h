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
    class ROCKY_EXPORT MBTilesImageLayer : public Inherit<ImageLayer, MBTilesImageLayer>
    {
    public:
        //! Construct an empty layer
        MBTilesImageLayer();
        explicit MBTilesImageLayer(const std::string& JSON, const IOOptions& io);

        //! Location of the mbtiles database file
        void setURI(const URI& value) { _options.uri = value; }
        optional<URI>& uri() { return _options.uri; }

        //! Content type of the individual tiles in the database (e.g., image/jpg)
        void setFormat(const std::string& value) { _options.format = value; }
        optional<std::string>& format() { return _options.format; }

        //! Whether to use compression on individual tile data
        void setCompress(bool value) { _options.compress = value; }
        optional<bool>& compress() { return _options.compress; }

        //! serialize
        JSON to_json() const override;

    protected:

        //! Opens the layer and returns its status
        Status openImplementation(const IOOptions& io) override;

        //! Closes the layer and returns its status
        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        Status writeImageImplementation(const TileKey& key, shared_ptr<Image> image, const IOOptions& io) const override;

    private:
        MBTiles::Driver _driver;
        MBTiles::Options _options;

        void construct(const std::string& JSON, const IOOptions& io);
    };
}

#endif // ROCKY_HAS_MBTILES
