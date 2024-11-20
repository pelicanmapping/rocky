/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#ifdef ROCKY_HAS_GDAL

#include <rocky/URI.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>
#include <rocky/GeoExtent.h>
#include <rocky/TileKey.h>

class GDALDataset;
class GDALRasterBand;

namespace ROCKY_NAMESPACE
{
    namespace GDAL
    {
        /**
         * Encapsulates a user-supplied GDALDataset
         */
        struct ExternalDataset
        {
            GDALDataset* dataset = nullptr;
            bool owns_dataset = false;
        };

        /**
        * Base class for GDAL layers.
        */
        struct ROCKY_EXPORT LayerBase
        {
        public:
            //! Base URL for TMS requests
            void setURI(const URI& value);
            const optional<URI>& uri() const;

            //! Database connection for GDAL database queries (alternative to URL)
            void setConnection(const std::string& value);
            const optional<std::string>& connection() const;

            //! GDAL sub-dataset index (optional)
            void setSubDataset(unsigned value);
            const optional<unsigned>& subDataset() const;

            //! Interpolation method for resampling (default is bilinear)
            void setInterpolation(const Image::Interpolation& value);
            const optional<Image::Interpolation>& interpolation() const;

        protected:
            optional<URI> _uri = { };
            optional<std::string> _connection = { };
            optional<unsigned> _subDataset = 0;
            optional<Image::Interpolation> _interpolation = Image::AVERAGE;
            optional<bool> _singleThreaded = false;
        };

        /**
         * Driver for reading raster data using GDAL.
         * It is rarely necessary to use this object directly; use a
         * GDALImageLayer or GDALElevationLayer instead.
         */
        class ROCKY_EXPORT Driver
        {
        public:
            optional<float> noDataValue;
            optional<float> minValidValue;
            optional<float> maxValidValue;
            optional<unsigned> maxDataLevel = 30;

            //! Constructs a new driver
            Driver() = default;

            virtual ~Driver();

            Driver(const Driver&) = delete;

            Driver(Driver&& rhs) noexcept {
                _srcDS = rhs._srcDS;
                _warpedDS = rhs._warpedDS;
                _linearUnits = rhs._linearUnits;
                _extents = rhs._extents;
                _bounds = rhs._bounds;
                _profile = rhs._profile;
                _layer = rhs._layer;
                _external = std::move(rhs._external);
                _name = std::move(rhs._name);
                _open = rhs._open;
                rhs._srcDS = nullptr;
                rhs._warpedDS = nullptr;
                rhs._open = false;
                rhs._profile = {};
                rhs._layer = nullptr;
                rhs._external = nullptr;
            }

            Driver& operator=(Driver&& rhs) noexcept = delete;
            
            bool isOpen() const {
                return _open;
            }

            //! Opens and initializes the connection to the dataset
            Status open(
                const std::string& name,
                const LayerBase* layer,
                unsigned tileSize,
                DataExtentList* out_dataExtents,
                const IOOptions& io);

            //! Creates an image if possible
            Result<std::shared_ptr<Image>> createImage(
                const TileKey& key,
                unsigned tileSize,
                const IOOptions& io);

            const Profile& profile() const {
                return _profile;
            }

        private:
            void pixelToGeo(double, double, double&, double&);
            void geoToPixel(double, double, double&, double&);

            bool isValidValue(float, GDALRasterBand*);
            float getValidElevationValue(float value, float nodataValueFromBand, float replacement);
            bool intersects(const TileKey&);
            float getInterpolatedValue(GDALRasterBand* band, double x, double y, bool applyOffset = true);

            bool _open = false;
            GDALDataset* _srcDS = nullptr;
            GDALDataset* _warpedDS = nullptr;
            double _linearUnits = 1.0;
            double _geotransform[6];
            double _invtransform[6];
            GeoExtent _extents;
            Box _bounds;
            Profile _profile;
            const LayerBase* _layer;
            std::shared_ptr<ExternalDataset> _external;
            std::string _name;
            std::thread::id _threadId;

            const std::string& getName() const { return _name; }
        };

        //! Reads an image from raw data using the specified GDAL driver.
        extern ROCKY_EXPORT Result<std::shared_ptr<Image>> readImage(
            unsigned char* data,
            std::size_t len,
            const std::string& gdal_driver);

    } // namespace GDAL

} // namespace ROCKY_NAMESPACE


#else // if !ROCKY_HAS_GDAL
#ifndef ROCKY_BUILDING_SDK
#error GDAL support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_GDAL
