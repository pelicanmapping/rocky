/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Version.h>
#include <rocky/URI.h>
#include <rocky/Image.h>
#include <rocky/TileKey.h>
#include <rocky/Profile.h>
#include <rocky/DateTime.h>

namespace ROCKY_NAMESPACE
{
    namespace TMS
    {
        struct Options
        {
            option<URI> uri;
            option<std::string> format;
            option<bool> invertY = false;
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
            std::string vsrsString = "egm96";
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
            bool invertYaxis = false;

            bool valid() const;
            void computeMinMaxLevel();
            void computeNumTiles();
            Profile createProfile() const;
            std::string getURI(const TileKey& key, bool invertY) const;
            bool intersectsKey(const TileKey& key) const;
            void generateTileSets(unsigned numLevels);

            TileMap() = default;

            TileMap(
                const std::string& url,
                const Profile& profile,
                const DataExtentList& dataExtents,
                const std::string& format,
                int tile_width,
                int tile_height);

            // working
            mutable std::size_t rotateIter = 0;
            std::string rotateString;
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
            Result<> open(
                const URI& uri,
                Profile& profile,
                const std::string& format,
                DataExtentList& out_dataExtents,
                const IOOptions& io);

            void close();

            Result<std::shared_ptr<Image>> read(
                const TileKey& key,
                bool invertY,
                bool isMapboxRGB,
                const URI::Context& context,
                const IOOptions& io) const;

            //! Source information structure
            TileMap tileMap;
        };
    }
}
