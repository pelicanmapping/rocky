/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/TMS.h>

#ifdef ROCKY_HAS_TMS

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
        TMSElevationLayer(const std::string& JSON, const IOOptions& io);

        //! Serialize
        std::string to_json() const override;

        optional<Encoding> encoding;

    public: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const override;

    private:
        TMS::Driver _driver;
        void construct(const std::string& JSON, const IOOptions& io);
    };
}

#else // if !ROCKY_HAS_TMS
#ifndef ROCKY_BUILDING_SDK
#error TMS support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_TMS
