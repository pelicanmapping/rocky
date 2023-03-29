/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/URI.h>
#include <rocky/Image.h>
#include <rocky/TileKey.h>
#include <rocky/Profile.h>

namespace ROCKY_NAMESPACE
{
    namespace TMS
    {
        struct Options
        {
            optional<URI> uri;
            optional<std::string> tmsType;
            optional<std::string> format;
            optional<bool> coverage = false;
        };

        struct TileFormat
        {
            unsigned width = 256u;
            unsigned height = 256u;
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
            double originX = 0.0, originY = 0.0;
            double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
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
                bool isMapboxRGB,
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

            URI createSubstitutionURI(const TileKey& key, const URI& uri, bool invert_y) const;

            //bool resolveWriter(const std::string& format);
        };
    }
}
