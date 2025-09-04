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
    class ROCKY_EXPORT TMSImageLayer : public Inherit<ImageLayer, TMSImageLayer>,
        public TMS::Options
    {
    public:
        //! Construct an empty TMS layer
        TMSImageLayer();
        TMSImageLayer(std::string_view JSON, const IOOptions& io);

        //! serialize
        std::string to_json() const override;

        //! extent
        const GeoExtent& extent() const override;

    protected: // Layer

        Result<> openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createTileImplementation(const TileKey& key, const IOOptions& io) const override;

    private:
        TMS::Driver _driver;

        void construct(std::string_view JSON, const IOOptions& io);
    };
}
