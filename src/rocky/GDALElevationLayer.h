/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/GDAL.h>
#ifdef ROCKY_HAS_GDAL

#include <rocky/ElevationLayer.h>

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
        GDALElevationLayer(const std::string& JSON, const IOOptions& io);

        //! serialize
        std::string to_json() const override;

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
        void construct(const std::string& JSON, const IOOptions& io);

        mutable util::ThreadLocal<GDAL::Driver> _drivers;
        friend class GDAL::Driver;
    };

} // namespace ROCKY_NAMESPACE

#endif // ROCKY_HAS_GDAL