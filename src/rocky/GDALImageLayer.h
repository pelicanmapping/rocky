/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/GDAL.h>
#ifdef ROCKY_HAS_GDAL

#include <rocky/ImageLayer.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Image layer connected to a GDAL raster dataset
     */
    class ROCKY_EXPORT GDALImageLayer : public Inherit<ImageLayer, GDALImageLayer>,
        public GDAL::Options
    {
    public:
        //! Construct a GDAL image layer
        GDALImageLayer();

        //! Deserialize a GDAL image layer
        GDALImageLayer(const std::string& JSON, const IOOptions& io);

        //! Serialize the layer
        std::string to_json() const override;

    protected: // Layer

        //! Establishes a connection to the GDAL data source
        Status openImplementation(const IOOptions&) override;

        //! Closes down any GDAL connections
        void closeImplementation() override;

        //! Gets a raster image for the given tile key
        Result<GeoImage> createImageImplementation(
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
