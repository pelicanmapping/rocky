/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/TMS.h>
#include <rocky/ElevationLayer.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Elevation layer reading from a TMS (Tile Map Service) endpoint
     */
    class ROCKY_EXPORT TMSElevationLayer : public Inherit<ElevationLayer, TMSElevationLayer>,
        public TMS::Options
    {
    public:
        //! Construct an empty TMS layer
        TMSElevationLayer();
        TMSElevationLayer(std::string_view JSON, const IOOptions& io);

        //! Serialize
        std::string to_json() const override;

        option<Encoding> encoding;

    public: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const override;

    private:
        TMS::Driver _driver;
        void construct(std::string_view JSON, const IOOptions& io);
    };
}
