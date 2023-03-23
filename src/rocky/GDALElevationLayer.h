/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/ElevationLayer.h>
#include <rocky/GDAL.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Elevation layer connected to a GDAL raster dataset
     */
    class ROCKY_EXPORT GDALElevationLayer : public Inherit<ElevationLayer, GDALElevationLayer>,
        public GDAL::LayerBase
    {
    public:
        //! Construct a GDAL image layer
        GDALElevationLayer();

        //! Deserialize a GDAL image layer
        GDALElevationLayer(const Config&);

        Config getConfig() const override;

    protected:

        //! Establishes a connection to the GDAL data source
        Status openImplementation(const IOOptions&) override;

        //! Closes down any GDAL connections
        void closeImplementation() override;

        //! Gets a raster heightfield for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(
            const TileKey& key,
            const IOOptions& io) const override;

    private:

        //! Called by the constructors
        void construct(const Config&);

        mutable util::ThreadLocal<shared_ptr<GDAL::Driver>> _drivers;
        friend class GDAL::Driver;
    };

} // namespace ROCKY_NAMESPACE
