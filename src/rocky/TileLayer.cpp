/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileLayer.h"
#include "TileKey.h"
#include "Map.h"
#include "rtree.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[TileLayer] \"" << name().value() << "\" "

namespace
{
    using DataExtentsIndex = RTree<DataExtent, double, 2>;
}

TileLayer::TileLayer() :
    super()
{
    construct(JSON());
}

TileLayer::TileLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
TileLayer::construct(const JSON& conf)
{
    const auto j = parse_json(conf);
    get_to(j, "max_level", _maxLevel);
    get_to(j, "max_resolution", _maxResolution);
    get_to(j, "max_data_level", _maxDataLevel);
    get_to(j, "min_level", _minLevel);
    get_to(j, "profile", _profile);
    get_to(j, "tile_size", _tileSize);

    _writingRequested = false;
    _dataExtentsIndex = nullptr;
}

JSON
TileLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "max_level", _maxLevel);
    set(j, "max_resolution", _maxResolution);
    set(j, "max_data_level", _maxDataLevel);
    set(j, "min_level", _minLevel);
    set(j, "profile", _profile);
    set(j, "tile_size", _tileSize);
    return j.dump();
}

TileLayer::~TileLayer()
{
    if (_dataExtentsIndex)
    {
        delete static_cast<DataExtentsIndex*>(_dataExtentsIndex);
    }    
}

void TileLayer::setMinLevel(unsigned value) {
    _minLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::minLevel() const {
    return _minLevel;
}
void TileLayer::setMaxLevel(unsigned value) {
    _maxLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::maxLevel() const {
    return _maxLevel;
}
void TileLayer::setMinResolution(double value) {
    _minResolution = value, _reopenRequired = true;
}
const optional<double>& TileLayer::minResolution() const {
    return _minResolution;
}
void TileLayer::setMaxResolution(double value) {
    _maxResolution = value, _reopenRequired = true;
}
const optional<double>& TileLayer::maxResolution() const {
    return _maxResolution;
}
void TileLayer::setMaxDataLevel(unsigned value) {
    _maxDataLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::maxDataLevel() const {
    return _maxDataLevel;
}
void TileLayer::setTileSize(unsigned value) {
    _tileSize = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::tileSize() const {
    return _tileSize;
}

Status
TileLayer::openImplementation(const IOOptions& io)
{
    auto result = super::openImplementation(io);
    if (result.ok())
    {
        //todo
    }
    return result;
}

#if 0
void
TileLayer::addedToMap(const Map* map)
{
    VisibleLayer::addedToMap(map);

    unsigned l2CacheSize = 0u;

    // If the profiles don't match, mosaicing will be likely so set up a
    // small L2 cache for this layer.
    if (map &&
        map->profile() &&
        profile() &&
        !map->profile().srs().isHorizEquivalentTo(profile().srs()))
    {
        l2CacheSize = 16u;
        //ROCKY_INFO << LC << "Map/Layer profiles differ; requesting L2 cache" << std::endl;
    }

    // Use the user defined option if it's set.
    if (_l2cachesize.has_value())
    {
        l2CacheSize = _l2cachesize.value();
    }

    setUpL2Cache(l2CacheSize);
}

void
TileLayer::setUpL2Cache(unsigned minSize)
{
    // Check the layer hints
    unsigned l2CacheSize = layerHints().L2CacheSize().getOrUse(minSize);

    // See if it was overridden with an env var.
    char const* l2env = ::getenv("ROCKY_L2_CACHE_SIZE");
    if (l2env)
    {
        l2CacheSize = util::as<int>(std::string(l2env), 0);
        //ROCKY_INFO << LC << "L2 cache size set from environment = " << l2CacheSize << "\n";
    }

    // Env cache-only mode also disables the L2 cache.
    char const* noCacheEnv = ::getenv("ROCKY_MEMORY_PROFILE");
    if (noCacheEnv)
    {
        l2CacheSize = 0;
    }

    // Initialize the l2 cache if it's size is > 0
    if (l2CacheSize > 0)
    {
//        _memCache = new MemCache(l2CacheSize);
        //ROCKY_INFO << LC << "L2 cache size = " << l2CacheSize << std::endl;
    }
}
#endif

const Status&
TileLayer::openForWriting()
{
    if (isWritingSupported())
    {
        _writingRequested = true;
        open();
        return status();
    }
    return setStatus(Status::ServiceUnavailable, "Layer does not support writing");
}

void
TileLayer::establishCacheSettings()
{
    //nop
}

const Profile&
TileLayer::profile() const
{
    return _profile;
}

void
TileLayer::setProfile(const Profile& profile)
{
    _profile = profile;
}

void
TileLayer::setProfileDefault(const Profile& profile)
{
    _profile.set_default(profile);
}

bool
TileLayer::dynamic() const
{
    if (hints().dynamic.has_value(true))
        return true;
    else
        return false;
}

std::string
TileLayer::getMetadataKey(const Profile& profile) const
{
    if (profile.valid())
        return make_string() << std::hex << profile.getHorizSignature() << "_metadata";
    else
        return "_metadata";
}

void
TileLayer::disable(const std::string& msg)
{
    setStatus(Status::Error(msg));
}

bool
TileLayer::isKeyInLegalRange(const TileKey& key) const
{
    if ( !key.valid() )
    {
        return false;
    }

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();


    // First check the key against the min/max level limits, it they are set.
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return false;
    }

    // Next check the maxDataLevel if that is set.
    if (_maxDataLevel.has_value() && localLOD > _maxDataLevel)
    {
        return false;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (profile().valid())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.extent().width() / (double)tileSize();
            double resLayer = SRS::transformUnits(resKey, key.profile().srs(), profile().srs(), Angle());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            {
                return false;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            {
                return false;
            }
        }
    }

    return true;
}

bool
TileLayer::isKeyInVisualRange(const TileKey& key) const
{
    if (!key.valid())
    {
        return false;
    }

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();


    // First check the key against the min/max level limits, it they are set.
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return false;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (profile().valid())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.extent().width() / (double)tileSize();
            double resLayer = SRS::transformUnits(resKey, key.profile().srs(), profile().srs(), Angle());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            {
                return false;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            {
                return false;
            }
        }
    }

    return true;
}

unsigned int TileLayer::dataExtentsSize() const
{
    std::shared_lock READ(_dataMutex);
    return _dataExtents.size();
}

DataExtentList
TileLayer::dataExtents() const
{
    std::shared_lock READ(_dataMutex);
    return _dataExtents;
}

void
TileLayer::setDataExtents(const DataExtentList& dataExtents)
{
    std::unique_lock WRITE(_dataMutex);
    _dataExtents = dataExtents;
    dirtyDataExtents();
}

void
TileLayer::addDataExtent(const DataExtent& dataExtent)
{
    std::unique_lock WRITE(_dataMutex);
    _dataExtents.push_back(dataExtent);
    dirtyDataExtents();
}

void
TileLayer::dirtyDataExtents()
{
    _dataExtentsUnion = GeoExtent::INVALID;

    if (_dataExtentsIndex)
    {
        delete static_cast<DataExtentsIndex*>(_dataExtentsIndex);
        _dataExtentsIndex = nullptr;
    }
}

const DataExtent&
TileLayer::dataExtentsUnion() const
{
    if (!_dataExtentsUnion.valid() && _dataExtents.size() > 0)
    {
        std::unique_lock WRITE(_dataMutex);
        {
            if (!_dataExtentsUnion.valid() && _dataExtents.size() > 0) // double-check
            {
                _dataExtentsUnion = _dataExtents[0];
                for (unsigned int i = 1; i < _dataExtents.size(); i++)
                {
                    _dataExtentsUnion.expandToInclude(_dataExtents[i]);

                    if (_dataExtents[i].minLevel().has_value())
                        _dataExtentsUnion.minLevel() = std::min(_dataExtentsUnion.minLevel().value(), _dataExtents[i].minLevel().value());

                    if (_dataExtents[i].maxLevel().has_value())
                        _dataExtentsUnion.maxLevel() = std::max(_dataExtentsUnion.maxLevel().value(), _dataExtents[i].maxLevel().value());
                }
            }
        }
    }
    return _dataExtentsUnion;
}

const GeoExtent&
TileLayer::extent() const
{
    return dataExtentsUnion();
}

TileKey
TileLayer::bestAvailableTileKey(
    const TileKey& key,
    bool considerUpsampling) const
{
    // trivial reject
    if (!key.valid())
    {
        return TileKey::INVALID;
    }

    unsigned MDL = _maxDataLevel;

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();

    // Check against level extrema:
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return TileKey::INVALID;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (profile().valid())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.extent().width() / (double)tileSize();
            double resLayer = SRS::transformUnits(resKey, key.profile().srs(), profile().srs(), Angle());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            {
                return TileKey::INVALID;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            {
                return TileKey::INVALID;
            }
        }
    }

    // If we have no data extents available, just return the MDL-limited input key.
    if (dataExtentsSize() == 0)
    {
        return localLOD > MDL ? key.createAncestorKey(MDL) : key;
    }

    // Reject if the extents don't overlap at all.
    // (Note: this does not consider min/max levels, only spatial extents)
    if (!dataExtentsUnion().intersects(key.extent()))
    {
        return TileKey::INVALID;
    }

    bool     intersects = false;
    unsigned highestLOD = 0;

    double a_min[2], a_max[2];
    // Build the index if needed.
    if (!_dataExtentsIndex)
    {
        std::unique_lock WRITE(_dataMutex);

        if (!_dataExtentsIndex) // Double check
        {
            //ROCKY_INFO << LC << "Building data extents index with " << _dataExtents.size() << " extents" << std::endl;
            DataExtentsIndex* dataExtentsIndex = new DataExtentsIndex();
            for (auto de = _dataExtents.begin(); de != _dataExtents.end(); ++de)
            {
                // Build the index in the SRS of this layer
                GeoExtent extentInLayerSRS = profile().clampAndTransformExtent(*de);

                if (extentInLayerSRS.srs().isGeodetic() && extentInLayerSRS.crossesAntimeridian())
                {
                    GeoExtent west, east;
                    extentInLayerSRS.splitAcrossAntimeridian(west, east);
                    if (west.valid())
                    {
                        DataExtent new_de(west);
                        new_de.minLevel() = de->minLevel();
                        new_de.maxLevel() = de->maxLevel();
                        a_min[0] = new_de.xMin(), a_min[1] = new_de.yMin();
                        a_max[0] = new_de.xMax(), a_max[1] = new_de.yMax();
                        dataExtentsIndex->Insert(a_min, a_max, new_de);
                    }
                    if (east.valid())
                    {
                        DataExtent new_de(east);
                        new_de.minLevel() = de->minLevel();
                        new_de.maxLevel() = de->maxLevel();
                        a_min[0] = new_de.xMin(), a_min[1] = new_de.yMin();
                        a_max[0] = new_de.xMax(), a_max[1] = new_de.yMax();
                        dataExtentsIndex->Insert(a_min, a_max, new_de);
                    }
                }
                else
                {
                    a_min[0] = extentInLayerSRS.xMin(), a_min[1] = extentInLayerSRS.yMin();
                    a_max[0] = extentInLayerSRS.xMax(), a_max[1] = extentInLayerSRS.yMax();
                    dataExtentsIndex->Insert(a_min, a_max, *de);
                }
            }
            _dataExtentsIndex = dataExtentsIndex;
        }
    }

    // Transform the key extent to the SRS of this layer to do the index search
    GeoExtent keyExtentInLayerSRS = profile().clampAndTransformExtent(key.extent());

    a_min[0] = keyExtentInLayerSRS.xMin(); a_min[1] = keyExtentInLayerSRS.yMin();
    a_max[0] = keyExtentInLayerSRS.xMax(); a_max[1] = keyExtentInLayerSRS.yMax();

    DataExtentsIndex* index = static_cast<DataExtentsIndex*>(_dataExtentsIndex);

    TileKey bestKey;
    index->Search(a_min, a_max, [&](const DataExtent& de) {
        // check that the extent isn't higher-resolution than our key:
        if (!de.minLevel().has_value() || localLOD >= (int)de.minLevel().value())
        {
            // Got an intersetion; now test the LODs:
            intersects = true;

            // If the maxLevel is not set, there's not enough information
            // so just assume our key might be good.
            if (!de.maxLevel().has_value())
            {
                bestKey = localLOD > MDL ? key.createAncestorKey(MDL) : key;
                return false; //Stop searching, we've found a key
            }

            // Is our key at a lower or equal LOD than the max key in this extent?
            // If so, our key is good.
            else if (localLOD <= (int)de.maxLevel().value())
            {
                bestKey = localLOD > MDL ? key.createAncestorKey(MDL) : key;
                return false; //Stop searching, we've found a key
            }

            // otherwise, record the highest encountered LOD that
            // intersects our key.
            else if (de.maxLevel().value() > highestLOD)
            {
                highestLOD = de.maxLevel().value();
            }                        
        }
        return true; // Continue searching
    });

    if (bestKey.valid())
    {
        return bestKey;
    }

    if ( intersects )
    {
        // for a normal dataset, dataset max takes priority over MDL.
        unsigned maxAvailableLOD = std::min(highestLOD, MDL);
        return key.createAncestorKey(std::min(key.levelOfDetail(), maxAvailableLOD));
    }

    return TileKey::INVALID;
}

void
TileLayer::buildDataExtentsIfNeeded() const
{
    // Build the index if needed.
    if (!_dataExtentsIndex)
    {
        std::unique_lock WRITE(_dataMutex);

        if (!_dataExtentsIndex) // Double check
        {
            double a_min[2], a_max[2];

            //ROCKY_INFO << LC << "Building data extents index with " << _dataExtents.size() << " extents" << std::endl;
            DataExtentsIndex* dataExtentsIndex = new DataExtentsIndex();
            for (auto de = _dataExtents.begin(); de != _dataExtents.end(); ++de)
            {
                // Build the index in the SRS of this layer
                GeoExtent extentInLayerSRS = profile().clampAndTransformExtent(*de);

                if (extentInLayerSRS.srs().isGeodetic() && extentInLayerSRS.crossesAntimeridian())
                {
                    GeoExtent west, east;
                    extentInLayerSRS.splitAcrossAntimeridian(west, east);
                    if (west.valid())
                    {
                        DataExtent new_de(west);
                        new_de.minLevel() = de->minLevel();
                        new_de.maxLevel() = de->maxLevel();
                        a_min[0] = new_de.xMin(), a_min[1] = new_de.yMin();
                        a_max[0] = new_de.xMax(), a_max[1] = new_de.yMax();
                        dataExtentsIndex->Insert(a_min, a_max, new_de);
                    }
                    if (east.valid())
                    {
                        DataExtent new_de(east);
                        new_de.minLevel() = de->minLevel();
                        new_de.maxLevel() = de->maxLevel();
                        a_min[0] = new_de.xMin(), a_min[1] = new_de.yMin();
                        a_max[0] = new_de.xMax(), a_max[1] = new_de.yMax();
                        dataExtentsIndex->Insert(a_min, a_max, new_de);
                    }
                }
                else
                {
                    a_min[0] = extentInLayerSRS.xMin(), a_min[1] = extentInLayerSRS.yMin();
                    a_max[0] = extentInLayerSRS.xMax(), a_max[1] = extentInLayerSRS.yMax();
                    dataExtentsIndex->Insert(a_min, a_max, *de);
                }
            }

            _dataExtentsIndex = dataExtentsIndex;
        }
    }
}

bool
TileLayer::intersects(const TileKey& key) const
{
    bool got_intersection = false;
    double a_min[2], a_max[2];

    buildDataExtentsIfNeeded();

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();

    // Transform the key extent to the SRS of this layer to do the index search
    GeoExtent keyExtentInLayerSRS = profile().clampAndTransformExtent(key.extent());

    a_min[0] = keyExtentInLayerSRS.xMin(); a_min[1] = keyExtentInLayerSRS.yMin();
    a_max[0] = keyExtentInLayerSRS.xMax(); a_max[1] = keyExtentInLayerSRS.yMax();

    DataExtentsIndex* index = static_cast<DataExtentsIndex*>(_dataExtentsIndex);

    return index->Search(a_min, a_max) > 0;
}

bool
TileLayer::mayHaveData(const TileKey& key) const
{
    bool yes = (key == bestAvailableTileKey(key, true));
    return yes;
}
