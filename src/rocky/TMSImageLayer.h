/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TMS.h>
#include <rocky/ImageLayer.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Image layer reading from a TMS (Tile Map Service) endpoint
     */
    class ROCKY_EXPORT TMSImageLayer : public Inherit<ImageLayer, TMSImageLayer>
    {
    public:
        //! Construct an empty TMS layer
        TMSImageLayer();
        TMSImageLayer(const Config&);

        //! Destructor
        virtual ~TMSImageLayer() { }

        //! Base URL for TMS requests
        void setURI(const URI& value) { _options.uri = value; }
        const optional<URI>& uri() const { return _options.uri; }

        //! Options TMS "type"; only possible setting is "google" which will
        //! invert the Y axis for tile indices
        void setTMSType(const std::string& value) { _options.tmsType = value; }
        const optional<std::string>& tmsType() const { return _options.tmsType; }

        //! Data format to request from the service
        void setFormat(const std::string& value) { _options.format = value; }
        const optional<std::string>& format() const { return _options.format; }

        //! serialize
        Config getConfig() const override;

    protected: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        //virtual Status writeImageImplementation(const TileKey& key, const osg::Image* image, ProgressCallback* progress) const override;

    private:
        TMS::Driver _driver;
        TMS::Options _options;

        void construct(const Config&);
    };
}
