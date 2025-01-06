/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#ifdef ROCKY_HAS_MBTILES

#include <rocky/Status.h>
#include <rocky/URI.h>
#include <rocky/TileKey.h>

namespace ROCKY_NAMESPACE
{
    class Image;

    namespace MBTiles
    {
        /**
        * Options for an MBTiles database
        */
        struct Options
        {
            //! Location of the mbtiles database file
            optional<URI> uri;

            //! Content type of the individual tiles in the database (e.g., image/tif)
            optional<std::string> format = { "image/png" };

            //! Whether to use compression on individual tile data
            optional<bool> compress = false;
        };

        /**
         * Underlying driver for reading/writing an MBTiles database
         */
        class ROCKY_EXPORT Driver
        {
        public:
            Driver();
            ~Driver();

            Status open(
                const std::string& name,
                const Options& options,
                bool isWritingRequested,
                Profile& profile_inout,
                DataExtentList& dataExtents,
                const IOOptions& io);

            void close();

            Result<std::shared_ptr<Image>> read(
                const TileKey& key,
                const IOOptions& io) const;

            Status write(
                const TileKey& key,
                std::shared_ptr<Image> image,
                const IOOptions& io) const;

            void setDataExtents(const DataExtentList&);
            bool getMetaData(const std::string& name, std::string& value);
            bool putMetaData(const std::string& name, const std::string& value);

        private:
            void* _database;
            mutable unsigned _minLevel;
            mutable unsigned _maxLevel;
            std::shared_ptr<Image> _emptyImage;
            Options _options;
            std::string _tileFormat;
            bool _forceRGB;
            std::string _name;

            // because no one knows if/when sqlite3 is threadsafe.
            mutable std::mutex _mutex;

            bool createTables();
            void computeLevels();
            Result<int> readMaxLevel();
        };
    }
}

#else // if !ROCKY_HAS_MBTILES
#ifndef ROCKY_BUILDING_SDK
#error MBTILES support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_MBTILES
