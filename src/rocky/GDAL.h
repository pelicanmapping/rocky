/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
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
            void setSubDataSet(unsigned value);
            const optional<unsigned>& subDataSet() const;

            //! Interpolation method for resampling (default is bilinear)
            void setInterpolation(const Image::Interpolation& value);
            const optional<Image::Interpolation>& interpolation() const;

            //! Use the new VRT read approach
            void setUseVRT(bool value);
            const optional<bool>& useVRT() const;

            //! Whether coverate data uses a palette index
            void setCoverageUsesPaletteIndex(bool value);
            const optional<bool>& coverageUsesPaletteIndex() const;

        protected:
            optional<URI> _uri = { };
            optional<std::string> _connection = { };
            optional<unsigned> _subDataSet = 0;
            optional<Image::Interpolation> _interpolation = Image::AVERAGE;
            optional<bool> _useVRT = false;
            optional<bool> _coverageUsesPaletteIndex = true;
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
            //! Constructs a new driver
            Driver();

            virtual ~Driver();

            //! Value to interpet as "no data"
            void setNoDataValue(float value) { _noDataValue = value; }

            //! Minimum valid data value (anything less is "no data")
            void setMinValidValue(float value) { _minValidValue = value; }

            //! Maximum valid data value (anything more is "no data")
            void setMaxValidValue(float value) { _maxValidValue = value; }

            //! Maximum LOD at which to return real data
            void setMaxDataLevel(unsigned value) { _maxDataLevel = value; }

            //! Opens and initializes the connection to the dataset
            Status open(
                const std::string& name,
                const LayerBase* layer,
                unsigned tileSize,
                DataExtentList* out_dataExtents,
                const IOOptions& io);

            //! Creates an image if possible
            Result<shared_ptr<Image>> createImage(
                const TileKey& key,
                unsigned tileSize,
                bool isCoverage,
                const IOOptions& io);

            //! Creates a heightfield if possible
            Result<shared_ptr<Heightfield>> createHeightfield(
                const TileKey& key,
                unsigned tileSize,
                const IOOptions& io);

            //! Creates a heightfield if possible using a faster path that creates a temporary warped VRT.
            Result<shared_ptr<Heightfield>> createHeightfieldWithVRT(
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
            bool intersects(const TileKey&);
            float getInterpolatedValue(GDALRasterBand* band, double x, double y, bool applyOffset = true);

            optional<float> _noDataValue, _minValidValue, _maxValidValue;
            optional<unsigned> _maxDataLevel;
            GDALDataset* _srcDS;
            GDALDataset* _warpedDS;
            double _linearUnits;
            double _geotransform[6];
            double _invtransform[6];
            GeoExtent _extents;
            Box _bounds;
            Profile _profile;
            const LayerBase* _layer;
            shared_ptr<ExternalDataset> _external;
            std::string _name;
            std::thread::id _threadId;

            const std::string& getName() const { return _name; }
        };

        //! Reads an image from raw data using the specified GDAL driver.
        extern ROCKY_EXPORT Result<shared_ptr<Image>> readImage(
            unsigned char* data, unsigned len, const std::string& gdal_driver);

#if 0
        extern ROCKY_EXPORT shared_ptr<Image> reprojectImage(
            const Image* srcImage,
            const std::string srcWKT,
            double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
            const std::string destWKT,
            double destMinX, double destMinY, double destMaxX, double destMaxY,
            int width = 0,
            int height = 0,
            bool useBilinearInterpolation = true);
#endif

    } // namespace GDAL

} // namespace ROCKY_NAMESPACE
