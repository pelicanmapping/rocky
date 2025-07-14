/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MBTiles.h"
#ifdef ROCKY_HAS_MBTILES

#include "Image.h"
#include "json.h"
#include "Context.h"
#include <filesystem>

#include <sqlite3.h>
ROCKY_ABOUT(sqlite, SQLITE_VERSION);

using namespace ROCKY_NAMESPACE;

#undef LC
#define LC "[MBTiles] "



MBTiles::Driver::Driver() :
    _minLevel(0),
    _maxLevel(19),
    _forceRGB(false),
    _database(nullptr)
{
    //nop
}

MBTiles::Driver::~Driver()
{
    close();
}

void
MBTiles::Driver::close()
{
    if (_database != nullptr)
    {
        sqlite3* database = (sqlite3*)_database;
        sqlite3_close_v2(database);
        _database = nullptr;
    }
}

Status
MBTiles::Driver::open(
    const std::string& name,
    const MBTiles::Options& options,
    bool isWritingRequested,
    Profile& profile,
    DataExtentList& out_dataExtents,
    const IOOptions& io)
{
    _name = name;

    std::string fullFilename = options.uri->full();

    bool readWrite = isWritingRequested;

    bool isNewDatabase = readWrite && !std::filesystem::exists(fullFilename);
    if (isNewDatabase)
    {
        // For a NEW database, the profile MUST be set prior to initialization.
        if (profile.valid() == false)
        {
            return Status(Status::ConfigurationError,
                "Cannot create database; required Profile is missing");
        }

        if (!options.format.has_value())
        {
            return Status(Status::ConfigurationError,
                "Cannot create database; required format property is missing");
        }

        //Log::info() << LC << "Database does not exist; attempting to create it." << std::endl;
    }

    // Try to open (or create) the database. We use SQLITE_OPEN_NOMUTEX to do
    // our own mutexing.
    int flags = readWrite
        ? (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX)
        : (SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX);

    sqlite3* database = (sqlite3*)_database;

    // close existing database if open
    close();

    sqlite3** dbptr = (sqlite3**)&_database;
    int rc = sqlite3_open_v2(fullFilename.c_str(), dbptr, flags, 0L);
    if (rc != 0)
    {
        return Status(Status::ResourceUnavailable,  util::make_string()
            << "Database \"" << fullFilename << "\": " << sqlite3_errmsg(database));
    }

    // New database setup:
    if (isNewDatabase)
    {
        // Make sure we have a readerwriter for the underlying tile format:
        _tileFormat = options.format.value();

        // create necessary db tables:
        createTables();

        // write profile to metadata:
        std::string profileJSON = profile.to_json();
        putMetaData("profile", profileJSON);

        // write format to metadata:
        putMetaData("format", _tileFormat);

        // compression?
        if (options.compress.has_value(true))
        {
            putMetaData("compression", "zlib");
        }

        // initialize and update as we write tiles.
        _minLevel = 0u;
        _maxLevel = 0u;
    }

    // If the database pre-existed, read in the information from the metadata.
    else // !isNewDatabase
    {
        computeLevels();
        //Log::info() << "Got levels from database " << _minLevel << ", " << _maxLevel << std::endl;

        std::string profileStr;
        getMetaData("profile", profileStr);

        // The data format (e.g., png, jpg, etc.). Any format passed in
        // in the options is superseded by the one in the database metadata.
        std::string metaDataFormat;
        getMetaData("format", metaDataFormat);
        if (!metaDataFormat.empty())
        {
            _tileFormat = metaDataFormat;
        }

        // Try to get it from the options.
        if (_tileFormat.empty())
        {
            if (options.format.has_value())
            {
                _tileFormat = options.format.value();
            }
        }
        else
        {
            // warn the user if the options format differs from the database format
            if (options.format.has_value() && options.format != _tileFormat)
            {
                Log()->warn(LC "Database tile format (" + _tileFormat + ") will override the layer options format ("
                    + options.format.value() + ")");
            }
        }

        // By this point, we require a valid tile format.
        if (_tileFormat.empty())
        {
            return Status(Status::ConfigurationError,
                "Required format not in metadata, nor specified in the options.");
        }

        // check for compression.
        std::string compression;
        if (getMetaData("compression", compression))
        {
            _options.compress = (compression == "zlib");
        }


        // Set the profile
        if (!profile.valid())
        {
            if (!profileStr.empty())
            {
                // try to parse it as a JSON config
                auto j = parse_json(profileStr);

                // new style. e.g., "global-geodetic"
                get_to(j, profile);

                // old style. e.g., {"profile":"global-geodetic"}
                if (!profile.valid())
                    get_to(j, "profile", profile);

                // if that didn't work, try parsing it directly
                if (!profile.valid())
                    profile = Profile(profileStr);
            }

            if (!profile.valid())
            {
                if (profileStr.empty() == false)
                {
                    Log()->warn(LC "Profile \"" + profileStr + "\" not recognized; defaulting to spherical-mercator");
                }

                profile = Profile("spherical-mercator");
            }
        }

        // Check for bounds and populate DataExtents.
        std::string boundsStr;
        if (getMetaData("bounds", boundsStr))
        {
            auto tokens = util::StringTokenizer()
                .delim(",")
                .tokenize(boundsStr);
            if (tokens.size() == 4)
            {
                double minLon = std::atof(tokens[0].c_str());
                double minLat = std::atof(tokens[1].c_str());
                double maxLon = std::atof(tokens[2].c_str());
                double maxLat = std::atof(tokens[3].c_str());

                GeoExtent extent;
                if (profile.valid())
                    extent = GeoExtent(profile.srs().geodeticSRS(), minLon, minLat, maxLon, maxLat);
                else
                    extent = GeoExtent(SRS::WGS84, minLon, minLat, maxLon, maxLat);

                if (extent.valid())
                {
                    // Using 0 for the minLevel is not technically correct, but we use it instead of the proper minLevel to force osgEarth to subdivide
                    // since we don't really handle DataExtents with minLevels > 0 just yet.
                    out_dataExtents.push_back(DataExtent(extent, 0, _maxLevel));
                }
                else
                {
                    Log()->warn(LC "MBTiles has invalid bounds " + extent.toString());
                }
            }
        }
        else
        {
            // Using 0 for the minLevel is not technically correct, but we use it instead of the proper minLevel to force osgEarth to subdivide
            // since we don't really handle DataExtents with minLevels > 0 just yet.
            out_dataExtents.push_back(DataExtent(profile.extent(), 0, _maxLevel));
        }
    }

    // do we require RGB? for jpeg?
    _forceRGB =
        util::endsWith(_tileFormat, "jpg", false) ||
        util::endsWith(_tileFormat, "jpeg", false);

    // make an empty image.
    int size = 256;
    _emptyImage = Image::create(Image::R8G8B8A8_UNORM, size, size);
    _emptyImage->fill(glm::fvec4(0.0));

    return StatusOK;
}

Result<int>
MBTiles::Driver::readMaxLevel()
{
    int result = -1;

    sqlite3* database = (sqlite3*)_database;

    //Get the image
    sqlite3_stmt* select = NULL;
    std::string query = "SELECT zoom_level FROM tiles ORDER BY zoom_level DESC LIMIT 1";
    int rc = sqlite3_prepare_v2(database, query.c_str(), -1, &select, 0L);
    if (rc != SQLITE_OK)
    {
        Log()->warn(LC "Failed to prepare SQL: " + query + "; " + sqlite3_errmsg(database));
        return Status::GeneralError;
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW)
    {
        result = sqlite3_column_int(select, 0);
    }

    sqlite3_finalize(select);
    return result;
}

Result<std::shared_ptr<Image>>
MBTiles::Driver::read(const TileKey& key, const IOOptions& io) const
{
    std::scoped_lock lock(_mutex);

    int z = key.level;
    int x = key.x;
    int y = key.y;

    if (z < (int)_minLevel)
    {
        return Status(Status::ResourceUnavailable);
    }

    if (z > (int)_maxLevel)
    {
        //If we're at the max level, just return NULL
        return Status(Status::ResourceUnavailable);
    }

    auto [numCols, numRows] = key.profile.numTiles(key.level);
    y = numRows - y - 1;

    sqlite3* database = (sqlite3*)_database;

    //Get the image
    sqlite3_stmt* select = NULL;
    std::string query = "SELECT tile_data from tiles where zoom_level = ? AND tile_column = ? AND tile_row = ?";
    int rc = sqlite3_prepare_v2(database, query.c_str(), -1, &select, 0L);
    if (rc != SQLITE_OK)
    {
        return Status(Status::GeneralError, util::make_string()
            << "Failed to prepare SQL: " << query << "; " << sqlite3_errmsg(database));
    }

    bool valid = true;

    sqlite3_bind_int(select, 1, z);
    sqlite3_bind_int(select, 2, x);
    sqlite3_bind_int(select, 3, y);

    Result<std::shared_ptr<Image>> result;
    std::string errorMessage;

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW)
    {
        // the pointer returned from _blob gets freed internally by sqlite, supposedly
        const char* data = (const char*)sqlite3_column_blob(select, 0);
        int dataLen = sqlite3_column_bytes(select, 0);

        std::string dataBuffer(data, dataLen);

#ifdef ROCKY_HAS_ZLIB
        // decompress if necessary:
        if (_options.compress == true)
        {
            std::istringstream inputStream(dataBuffer);
            std::string value;

            if (!util::ZLibCompressor().decompress(inputStream, value))
            {
                errorMessage = "Decompression failed";
                valid = false;
            }
            else
            {
                dataBuffer = value;
            }
        }
#endif // ROCKY_HAS_ZLIB

        // decode the raw image data:
        if (valid)
        {
            std::istringstream inputStream(dataBuffer);
            result = io.services.readImageFromStream(inputStream, {}, io);
        }
    }

    sqlite3_finalize(select);

    if (!valid)
    {
        result = Status(Status::GeneralError, errorMessage);
    }

    return result;
}


Status
MBTiles::Driver::write(const TileKey& key, std::shared_ptr<Image> input, const IOOptions& io) const
{
    if (!key.valid() || !input)
        return Status(Status::AssertionFailure);

    if (!io.services.writeImageToStream)
        return Status(Status::ServiceUnavailable);

    std::scoped_lock lock(_mutex);

    // encode the data stream:
    std::stringstream buf;

    // convert to RGB if we are storing jpgs (for example)
    auto image_to_write = input;
    if (_forceRGB && input->pixelFormat() == Image::R8G8B8A8_UNORM)
    {
        image_to_write = Image::create(Image::R8G8B8_UNORM, input->width(), input->height(), input->depth());

        input->eachPixel([&](auto& iter)
            {
                glm::fvec4 pixel;
                input->read(pixel, iter);
                image_to_write->write(pixel, iter);
            }
        );
    }

    Status wr = io.services.writeImageToStream(image_to_write, buf, _options.format, io);

    if (wr.failed())
    {
        return wr;
    }

    std::string value = buf.str();

#ifdef ROCKY_HAS_ZLIB
    // compress the buffer if necessary
    if (_options.compress == true)
    {
        std::ostringstream output;
        if (!util::ZLibCompressor().compress(value, output))
        {
            return Status(Status::GeneralError, "Compressor failed");
        }
        value = output.str();
    }
#endif // ROCKY_HAS_ZLIB

    int z = key.level;
    int x = key.x;
    int y = key.y;

    // flip Y axis
    auto [numCols, numRows] = key.profile.numTiles(key.level);
    y = numRows - y - 1;

    sqlite3* database = (sqlite3*)_database;

    // Prep the insert statement:
    sqlite3_stmt* insert = NULL;
    std::string query = "INSERT OR REPLACE INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?)";
    int rc = sqlite3_prepare_v2(database, query.c_str(), -1, &insert, 0L);
    if (rc != SQLITE_OK)
    {
        return Status(Status::GeneralError, util::make_string()
            << "Failed to prepare SQL: " << query << "; " << sqlite3_errmsg(database));
    }

    // bind parameters:
    sqlite3_bind_int(insert, 1, z);
    sqlite3_bind_int(insert, 2, x);
    sqlite3_bind_int(insert, 3, y);

    // bind the data blob:
    sqlite3_bind_blob(insert, 4, value.c_str(), (int)value.length(), SQLITE_STATIC);

    // run the sql.
    bool ok = true;
    int tries = 0;
    do {
        rc = sqlite3_step(insert);
    } while (++tries < 100 && (rc == SQLITE_BUSY || rc == SQLITE_LOCKED));

    if (SQLITE_OK != rc && SQLITE_DONE != rc)
    {
#if SQLITE_VERSION_NUMBER >= 3007015
        return Status(Status::GeneralError, util::make_string() << "Failed query: " << query << "(" << rc << ")" << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(database));
#else
        return Status(Status::GeneralError, make_string() << "Failed query: " << query << "(" << rc << ")" << rc << "; " << sqlite3_errmsg(database));
#endif
        ok = false;
    }

    sqlite3_finalize(insert);

    // adjust the max level if necessary
    if (key.level > _maxLevel)
    {
        _maxLevel = key.level;
    }
    if (key.level < _minLevel)
    {
        _minLevel = key.level;
    }

    return StatusOK;
}

bool
MBTiles::Driver::getMetaData(const std::string& key, std::string& value)
{
    std::scoped_lock lock(_mutex);

    sqlite3* database = (sqlite3*)_database;

    //get the metadata
    sqlite3_stmt* select = NULL;
    std::string query = "SELECT value from metadata where name = ?";
    int rc = sqlite3_prepare_v2(database, query.c_str(), -1, &select, 0L);
    if (rc != SQLITE_OK)
    {
        Log()->warn(LC "Failed to prepare SQL: " + query + "; " + sqlite3_errmsg(database));
        return false;
    }


    bool valid = true;
    std::string keyStr = std::string(key);
    rc = sqlite3_bind_text(select, 1, keyStr.c_str(), (int)keyStr.length(), SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        Log()->warn(LC "Failed to bind text: " + query + "; " + sqlite3_errmsg(database));
        return false;
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW)
    {
        value = (char*)sqlite3_column_text(select, 0);
    }

    sqlite3_finalize(select);
    return valid;
}

bool
MBTiles::Driver::putMetaData(const std::string& key, const std::string& value)
{
    std::scoped_lock lock(_mutex);

    sqlite3* database = (sqlite3*)_database;

    // prep the insert statement.
    sqlite3_stmt* insert = 0L;
    std::string query = "INSERT OR REPLACE INTO metadata (name,value) VALUES (?,?)";
    if (SQLITE_OK != sqlite3_prepare_v2(database, query.c_str(), -1, &insert, 0L))
    {
        Log()->warn(LC "Failed to prepare SQL: " + query + "; " + sqlite3_errmsg(database));
        return false;
    }

    // bind the values:
    if (SQLITE_OK != sqlite3_bind_text(insert, 1, key.c_str(), (int)key.length(), SQLITE_STATIC))
    {
        Log()->warn(LC "Failed to bind text: " + query + "; " + sqlite3_errmsg(database));
        return false;
    }
    if (SQLITE_OK != sqlite3_bind_text(insert, 2, value.c_str(), (int)value.length(), SQLITE_STATIC))
    {
        Log()->warn(LC "Failed to bind text: " + query + "; " + sqlite3_errmsg(database));
        return false;
    }

    // execute the sql. no idea what a good return value should be :/
    sqlite3_step(insert);
    sqlite3_finalize(insert);
    return true;
}

void
MBTiles::Driver::computeLevels()
{
    sqlite3* database = (sqlite3*)_database;
    sqlite3_stmt* select = nullptr;
    // Get min and max as separate queries to allow the SQLite query planner to convert it to a fast equivalent.
    std::string query = "SELECT (SELECT min(zoom_level) FROM tiles), (SELECT max(zoom_level) FROM tiles); ";
    int rc = sqlite3_prepare_v2(database, query.c_str(), -1, &select, 0L);
    if (rc != SQLITE_OK)
    {
        Log()->warn(LC "Failed to prepare SQL: " + query + "; " + sqlite3_errmsg(database));
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW)
    {
        _minLevel = sqlite3_column_int(select, 0);
        _maxLevel = sqlite3_column_int(select, 1);
    }

    sqlite3_finalize(select);
}

bool
MBTiles::Driver::createTables()
{
    // https://github.com/mapbox/mbtiles-spec/blob/master/1.2/spec.md

    sqlite3* database = (sqlite3*)_database;

    std::string query =
        "CREATE TABLE IF NOT EXISTS metadata ("
        " name text PRIMARY KEY,"
        " value text)";

    if (SQLITE_OK != sqlite3_exec(database, query.c_str(), 0L, 0L, 0L))
    {
        Log()->warn(LC "Failed to create table [metadata]");
        return false;
    }

    query =
        "CREATE TABLE IF NOT EXISTS tiles ("
        " zoom_level integer,"
        " tile_column integer,"
        " tile_row integer,"
        " tile_data blob)";

    char* errorMsg = 0L;

    if (SQLITE_OK != sqlite3_exec(database, query.c_str(), 0L, 0L, &errorMsg))
    {
        Log()->warn(LC "Failed to create table [tiles]: " + std::string(errorMsg));
        sqlite3_free(errorMsg);
        return false;
    }

    // create an index
    query =
        "CREATE UNIQUE INDEX tile_index ON tiles ("
        " zoom_level, tile_column, tile_row)";

    if (SQLITE_OK != sqlite3_exec(database, query.c_str(), 0L, 0L, &errorMsg))
    {
        Log()->warn(LC "Failed to create index on table [tiles]: " + std::string(errorMsg));
        sqlite3_free(errorMsg);
        // keep going... non-fatal
        // return false;
    }

    // TODO: support "grids" and "grid_data" tables if necessary.

    return true;
}

void
MBTiles::Driver::setDataExtents(const DataExtentList& values)
{
    if (_database != NULL && values.size() > 0)
    {
        // Get the union of all the extents
        GeoExtent e(values[0]);
        for (unsigned int i = 1; i < values.size(); i++)
        {
            e.expandToInclude(values[i]);
        }

        // Convert the bounds to geographic
        GeoExtent bounds;
        if (e.srs().isGeodetic())
        {
            bounds = e;
        }
        else
        {
            bounds = Profile("global-geodetic").clampAndTransformExtent(e);
        }
        std::stringstream boundsStr;
        boundsStr << bounds.xmin() << "," << bounds.ymin() << "," << bounds.xmax() << "," << bounds.ymax();
        putMetaData("bounds", boundsStr.str());
    }
}

#endif // ROCKY_HAS_MBTILES
