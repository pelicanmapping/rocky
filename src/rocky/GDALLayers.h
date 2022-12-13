/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/ImageLayer.h>
#include <rocky/ElevationLayer.h>
#include <rocky/URI.h>
#include <rocky/Image.h>

/**
 * GDAL (Geospatial Data Abstraction Library) Layers
 */
class GDALDataset;
class GDALRasterBand;


namespace rocky
{
    namespace GDAL
    {
        /**
         * Encapsulates a user-supplied GDALDataset
         */
        class ROCKY_EXPORT ExternalDataset // NO EXPORT; header only
        {
        public:
            ExternalDataset() : _dataset(NULL), _ownsDataset(true) {};
            ExternalDataset(GDALDataset* dataset, bool ownsDataset) : _dataset(dataset), _ownsDataset(ownsDataset) {};

        protected:
            virtual ~ExternalDataset() {};

        public:
            GDALDataset* dataset() const { return _dataset; };
            void setDataset(GDALDataset* dataset) { _dataset = dataset; };

            bool ownsDataset() const { return _ownsDataset; };
            void setOwnsDataset(bool ownsDataset) { _ownsDataset = ownsDataset; };

        private:
            GDALDataset* _dataset;
            bool _ownsDataset;
        };

        // GDAL-specific serialization data to be incorpoated by the LayerOptions below
        class ROCKY_EXPORT Options
        {
        public:
            Options() { }
            //Options(const ConfigOptions& input);
            optional<URI> url;
            optional<std::string> connection;
            optional<unsigned> subDataSet;
            optional<Image::Interpolation> interpolation;
            //optional<ProfileOptions> warpProfile;
            optional<bool> useVRT;
            optional<bool> coverageUsesPaletteIndex;
            optional<bool> singleThreaded;

            void readFrom(const Config& conf);
            void writeTo(Config& conf) const;
        };

        /**
         * Driver for reading raster data using GDAL.
         * It is rarely necessary to use this object directly; use a
         * GDALImageLayer or GDALElevationLayer instead.
         */
        class ROCKY_EXPORT Driver
        {
        public:
            using Ptr = std::shared_ptr<Driver>;

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
                const Options& options,
                unsigned tileSize,
                DataExtentList* out_dataExtents,
                const IOOptions& io);

            //! Creates an image if possible
            shared_ptr<Image> createImage(
                const TileKey& key,
                unsigned tileSize,
                bool isCoverage,
                const IOOptions& io);

            //! Creates a heightfield if possible
            shared_ptr<Heightfield> createHeightfield(
                const TileKey& key,
                unsigned tileSize,
                const IOOptions& io);

            //! Creates a heightfield if possible using a faster path that creates a temporary warped VRT.
            shared_ptr<Heightfield> createHeightfieldWithVRT(
                const TileKey& key,
                unsigned tileSize,
                const IOOptions& io);

            shared_ptr<Profile> getProfile() const {
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
            shared_ptr<Profile> _profile;
            Options _gdalOptions;
            const Options& gdalOptions() const { return _gdalOptions; }
            shared_ptr<GDAL::ExternalDataset> _externalDataset;
            std::string _name;
            unsigned _threadId;

            const std::string& getName() const { return _name; }
        };

        //! Creates an OSG image from an entire GDAL dataset
        extern ROCKY_EXPORT shared_ptr<Image> reprojectImage(
            const Image* srcImage,
            const std::string srcWKT,
            double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
            const std::string destWKT,
            double destMinX, double destMinY, double destMaxX, double destMaxY,
            int width = 0,
            int height = 0,
            bool useBilinearInterpolation = true);


        struct ROCKY_EXPORT LayerBase
        {
        protected:
            mutable util::Mutex _driversMutex;
            mutable util::Mutex _singleThreadingMutex;
            mutable std::unordered_map<unsigned, Driver::Ptr> _drivers;
            mutable GDAL::Options _options;

        public:

            //! Base URL for TMS requests
            void setURI(const URI& value);
            const URI& getURL() const;

            //! Database connection for GDAL database queries (alternative to URL)
            void setConnection(const std::string& value);
            const std::string& getConnection() const;

            //! GDAL sub-dataset index (optional)
            void setSubDataSet(unsigned value);
            unsigned getSubDataSet() const;

            //! Use a single-threaded driver (default is multi-threaded)
            void setSingleThreaded(bool value);
            bool getSingleThreaded() const;

            //! Interpolation method for resampling (default is bilinear)
            void setInterpolation(const Image::Interpolation& value);
            const Image::Interpolation& getInterpolation() const;

            //! Use the new VRT read approach
            void setUseVRT(bool value);
            bool getUseVRT() const;
        };
    }
}



namespace rocky
{
    /**
     * Image layer connected to a GDAL raster dataset
     */
    class ROCKY_EXPORT GDALImageLayer :
        public Inherit<ImageLayer, GDALImageLayer>,
        public GDAL::LayerBase
    {
    //public: // serialization
    //    class ROCKY_EXPORT Options : public ImageLayer::Options, public GDAL::Options {
    //    public:
    //        ROCKY_LayerOptions(Options, ImageLayer::Options);
    //        virtual Config getConfig() const;
    //    private:
    //        void fromConfig(const Config&);
    //    };

    public:
        GDALImageLayer();

        GDALImageLayer(const Config&);

        //ROCKY_Layer(GDALImageLayer, Options, GDALImage);

    public: // Layer

        //! Establishes a connection to the TMS repository
        virtual Status openImplementation(const IOOptions&) override;

        //! Closes down any GDAL connections
        virtual Status closeImplementation() override;

        //! Gets a raster image for the given tile key
        virtual Result<GeoImage> createImageImplementation(
            const TileKey& key,
            const IOOptions& io) const override;

        virtual Config getConfig() const override;

    private:

        //! Called by the constructors
        void construct(const Config&);
    };


    //! Elevation layer connected to a GDAL facility
    class ROCKY_EXPORT GDALElevationLayer :
        public Inherit<ElevationLayer, GDALElevationLayer>,
        public GDAL::LayerBase
    {
    //public: 
    //    class ROCKY_EXPORT Options : public ElevationLayer::Options, public GDAL::Options {
    //    public:
    //        ROCKY_LayerOptions(Options, ElevationLayer::Options);
    //        virtual Config getConfig() const;
    //    private:
    //        void fromConfig(const Config&);
    //    };

    public:
        GDALElevationLayer();

        GDALElevationLayer(const Config& conf);


        virtual Config getConfig() const override;

    public: // Layer

        //! Establishes a connection to the repository
        virtual Status openImplementation(const IOOptions&) override;

        //! Closes down any GDAL connections
        virtual Status closeImplementation() override;

        //! Gets a heightfield for the given tile key
        virtual Result<GeoHeightfield> createHeightfieldImplementation(
            const TileKey& key,
            const IOOptions& io) const override;

    private:

        //! Called by the constructor
        void construct(const Config&);
    };

} // namespace rocky
