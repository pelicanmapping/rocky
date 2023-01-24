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

namespace ROCKY_NAMESPACE
{
    namespace TMS
    {
        struct TileFormat
        {
            unsigned width = 0u;
            unsigned height = 0u;
            std::string mimeType;
            std::string extension;
        };

        struct TileSet
        {
            std::string href;
            double unitsPerPixel = 0.0;
            unsigned order = 0u;
        };

        enum class ProfileType
        {
            UNKNOWN,
            GEODETIC,
            MERCATOR,
            LOCAL
        };

        struct ROCKY_EXPORT TileMap
        {
            std::string tileMapService;
            std::string version;
            std::string title;
            std::string abstract;
            std::string srsString;
            std::string vsrsString;
            double originX, originY;
            double minX, minY, maxX, maxY;
            std::vector<TileSet> tileSets;
            TileFormat format;
            std::string filename;
            unsigned minLevel = 0u;
            unsigned maxLevel = 99u;
            unsigned numTilesWide = 0u;
            unsigned numTilesHigh = 0u;
            ProfileType profileType = ProfileType::UNKNOWN;
            TimeStamp timestamp;
            DataExtentList dataExtents;

            bool valid() const;
            void computeMinMaxLevel();
            void computeNumTiles();
            Profile createProfile() const;
            std::string getURI(const TileKey& key, bool invertY) const;
            bool intersectsKey(const TileKey& key) const;
            void generateTileSets(unsigned numLevels);

            TileMap() { }

            TileMap(
                const std::string& url,
                const Profile& profile,
                const DataExtentList& dataExtents,
                const std::string& format,
                int tile_width,
                int tile_height);
        };

        struct TileMapEntry
        {
            std::string title;
            std::string href;
            std::string srs;
            std::string profile;
        };

        using TileMapEntries = std::list<TileMapEntry>;

        extern Result<TileMap> readTileMap(const URI& location, const IOOptions& io);

        extern TileMapEntries readTileMapEntries(const URI& location, const IOOptions& io);


        /**
         * Underlying TMS driver that does the actual TMS I/O
         */
        class ROCKY_EXPORT Driver
        {
        public:
            Status open(
                const URI& uri,
                Profile& profile,
                const std::string& format,
                bool isCoverage,
                DataExtentList& out_dataExtents,
                const IOOptions& io);

            void close();

            Result<shared_ptr<Image>> read(
                const URI& uri,
                const TileKey& key,
                bool invertY,
                const IOOptions& io) const;

            bool write(
                const URI& uri,
                const TileKey& key,
                shared_ptr<Image> image,
                bool invertY,
                IOOptions& io) const;

        private:
            TileMap _tileMap;
            bool _forceRGBWrites;
            bool _isCoverage;

            //bool resolveWriter(const std::string& format);
        };

#if 0
        /**
         * Serializable TMS options
         */
        class ROCKY_EXPORT Options
        {
        public:
            URI uri;
            std::string tmsType;
            std::string format;
            ROCKY_OPTION(URI, url);
            ROCKY_OPTION(std::string, tmsType);
            ROCKY_OPTION(std::string, format);
            static Config getMetadata();
            void readFrom(const Config& conf);
            void writeTo(Config&) const;
        };
#endif
    }



    /**
     * Image layer connected to a TMS (Tile Map Service) facility
     */
    class ROCKY_EXPORT TMSImageLayer : public Inherit<ImageLayer, TMSImageLayer>
    {
    public:
        TMSImageLayer();

        //! Destructor
        virtual ~TMSImageLayer() { }

        //! Base URL for TMS requests
        void setURL(const URI& value) { _uri = value; }
        const URI& uri() const { return _uri; }

        //! Options TMS "type"; only possible setting is "google" which will
        //! invert the Y axis for tile indices
        void setTMSType(const std::string& value) { _tmsType = value; }
        const std::string& tmsType() const { return _tmsType; }

        //! Data format to request from the service
        void setFormat(const std::string& value) { _format = value; }
        const std::string& format() const { return _format; }

        //! Whether the layer contains coverage data
        void setCoverage(bool value) { _coverage = value; }
        bool coverage() const { return _coverage; }

    public: // Layer

        Status openImplementation(const IOOptions& io) override;

        Status closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        //virtual Status writeImageImplementation(const TileKey& key, const osg::Image* image, ProgressCallback* progress) const override;

    protected: // Layer

        //! Called by constructors
        //virtual void init() override;

        //virtual bool isWritingSupported() const override { return true; }


    private:
        TMS::Driver _driver;
        mutable util::ReadWriteMutex _mutex;
        URI _uri;
        std::string _tmsType;
        std::string _format;
        bool _coverage;
    };
}


#if 0
    /**
     * Elevation layer connected to a TMS (Tile Map Service) facility
     */
    class OSGEARTH_EXPORT TMSElevationLayer : public ElevationLayer
    {
    public: // serialization
        class OSGEARTH_EXPORT Options : public ElevationLayer::Options, public TMS::Options {
        public:
            META_LayerOptions(osgEarth, Options, ElevationLayer::Options);
            virtual Config getConfig() const;
        private:
            void fromConfig(const Config& conf);
        };

    public:
        META_Layer(osgEarth, TMSElevationLayer, Options, ElevationLayer, TMSElevation);

        //! Base URL for TMS requests
        void setURL(const URI& value);
        const URI& getURL() const;

        //! Options TMS "type"; only possible setting is "google" which will
        //! invert the Y axis for tile indices
        void setTMSType(const std::string& value);
        const std::string& getTMSType() const;

        //! Data format to request from the service
        void setFormat(const std::string& value);
        const std::string& getFormat() const;

    public: // Layer
        
        //! Establishes a connection to the TMS repository
        virtual Status openImplementation() override;
        virtual Status closeImplementation() override;

        //! Creates a heightfield for the given tile key
        virtual GeoHeightField createHeightFieldImplementation(const TileKey& key, ProgressCallback* progress) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        virtual Status writeHeightFieldImplementation(const TileKey& key, const osg::HeightField* hf, ProgressCallback* progress) const override;

    protected: // Layer

        //! Called by constructors
        virtual void init() override;

        virtual bool isWritingSupported() const override { return true; }

    protected:

        //! Destructor
        virtual ~TMSElevationLayer() { }

    private:
        osg::ref_ptr<TMSImageLayer> _imageLayer;
    };

} // namespace osgEarth

OSGEARTH_SPECIALIZE_CONFIG(osgEarth::TMSImageLayer::Options);
OSGEARTH_SPECIALIZE_CONFIG(osgEarth::TMSElevationLayer::Options);

#endif