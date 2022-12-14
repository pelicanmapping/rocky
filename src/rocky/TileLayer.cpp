/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileLayer.h"
#include "TileKey.h"
#include "Map.h"
#include "rtree.h"

using namespace rocky;
using namespace rocky::util;

namespace
{
    using DataExtentsIndex = RTree<DataExtent, double, 2>;
}

#define LC "[TileLayer] \"" << name().value() << "\" "

//------------------------------------------------------------------------

#if 0
Config
TileLayer::Options::getConfig() const
{
    Config conf = VisibleLayer::Options::getConfig();

    conf.set("max_level", _maxLevel);
    conf.set("max_resolution", _maxResolution);
    conf.set("max_valid_value", _maxValidValue);
    conf.set("max_data_level", _maxDataLevel);
    conf.set("min_level", _minLevel);
    conf.set("min_valid_value", _minValidValue);
    conf.set("min_resolution", _minResolution);
    conf.set("no_data_value", _noDataValue);
    conf.set("profile", _profile);
    conf.set("tile_size", _tileSize);
    conf.set("upsample", upsample());

    return conf;
}

void
TileLayer::Options::fromConfig(const Config& conf)
{
    _minLevel.init( 0 );
    _maxLevel.init( 23 );
    _maxDataLevel.init( 99 );
    _tileSize.init( 256 );
    _noDataValue.init( -32767.0f ); // SHRT_MIN
    _minValidValue.init( -32766.0f ); // -(2^15 - 2)
    _maxValidValue.init( 32767.0f );
    upsample().setDefault(false);

    conf.get( "min_level", _minLevel );
    conf.get( "max_level", _maxLevel );
    conf.get( "min_resolution", _minResolution );
    conf.get( "max_resolution", _maxResolution );
    conf.get( "max_data_level", _maxDataLevel );
    conf.get( "tile_size", _tileSize);
    conf.get( "profile", _profile);
    conf.get( "no_data_value", _noDataValue);
    conf.get( "nodata_value", _noDataValue); // back compat
    conf.get( "min_valid_value", _minValidValue);
    conf.get( "max_valid_value", _maxValidValue);
    conf.get("upsample", upsample());
}
#endif

//------------------------------------------------------------------------

TileLayer::CacheBinMetadata::CacheBinMetadata() :
    _valid(false)
{
    //nop
}

TileLayer::CacheBinMetadata::CacheBinMetadata(const TileLayer::CacheBinMetadata& rhs) :
    _valid(rhs._valid),
    _cacheBinId(rhs._cacheBinId),
    _sourceName(rhs._sourceName),
    _sourceDriver(rhs._sourceDriver),
    _sourceTileSize(rhs._sourceTileSize),
    _sourceProfile(rhs._sourceProfile),
    _cacheProfile(rhs._cacheProfile),
    _cacheCreateTime(rhs._cacheCreateTime)
{
    //nop
}

TileLayer::CacheBinMetadata::CacheBinMetadata(const Config& conf)
{
    _valid = !conf.empty();

    conf.get("cachebin_id", _cacheBinId);
    conf.get("source_name", _sourceName);
    conf.get("source_driver", _sourceDriver);
    conf.get("source_tile_size", _sourceTileSize);
    conf.get("source_profile", _sourceProfile);
    conf.get("cache_profile", _cacheProfile);
    conf.get("cache_create_time", _cacheCreateTime);

    const Config* extentsRoot = conf.child_ptr("extents");
    if ( extentsRoot )
    {
        const ConfigSet& extents = extentsRoot->children();

        for (ConfigSet::const_iterator i = extents.begin(); i != extents.end(); ++i)
        {
            std::string srsString;
            double xmin, ymin, xmax, ymax;
            optional<unsigned> minLevel, maxLevel;

            srsString = i->value("srs");
            xmin = i->value("xmin", 0.0f);
            ymin = i->value("ymin", 0.0f);
            xmax = i->value("xmax", 0.0f);
            ymax = i->value("ymax", 0.0f);
            i->get("minlevel", minLevel);
            i->get("maxlevel", maxLevel);

            auto srs = SRS::get(srsString);
            DataExtent e(GeoExtent(srs, xmin, ymin, xmax, ymax));
            if (minLevel.has_value())
                e.minLevel() = minLevel.get();
            if (maxLevel.has_value())
                e.maxLevel() = maxLevel.get();

            _dataExtents.push_back(e);
        }
    }

    // check for validity. This will reject older caches that don't have
    // sufficient attribution.
    if (_valid)
    {
        if (!conf.hasValue("source_tile_size") ||
            !conf.hasChild("source_profile") ||
            !conf.hasChild("cache_profile"))
        {
            _valid = false;
        }
    }
}

Config
TileLayer::CacheBinMetadata::getConfig() const
{
    Config conf("osgearth_terrainlayer_cachebin");
    conf.set("cachebin_id", _cacheBinId);
    conf.set("source_name", _sourceName);
    conf.set("source_driver", _sourceDriver);
    conf.set("source_tile_size", _sourceTileSize);
    conf.set("source_profile", _sourceProfile);
    conf.set("cache_profile", _cacheProfile);
    conf.set("cache_create_time", _cacheCreateTime);

    if (!_dataExtents.empty())
    {
        Config extents;
        for (DataExtentList::const_iterator i = _dataExtents.begin(); i != _dataExtents.end(); ++i)
        {
            Config extent;
            extent.set("srs", i->getSRS()->getHorizInitString());
            extent.set("xmin", i->xMin());
            extent.set("ymin", i->yMin());
            extent.set("xmax", i->xMax());
            extent.set("ymax", i->yMax());
            extent.set("minlevel", i->minLevel());
            extent.set("maxlevel", i->maxLevel());

            extents.add("extent", extent);
        }
        conf.add("extents", extents);
    }

    return conf;
}

//------------------------------------------------------------------------

TileLayer::TileLayer() :
    super()
{
    construct(Config());
}

TileLayer::TileLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
TileLayer::construct(const Config& conf)
{
    _minLevel.setDefault(0);
    _maxLevel.setDefault(23);
    _maxDataLevel.setDefault(99);
    _tileSize.setDefault(256);
    //_noDataValue.init(-32767.0f); // SHRT_MIN
    //_minValidValue.init(-32766.0f); // -(2^15 - 2)
    //_maxValidValue.init(32767.0f);
    //upsample().setDefault(false);

    conf.get("max_level", _maxLevel);
    conf.get("max_resolution", _maxResolution);
    //conf.get("max_valid_value", _maxValidValue);
    conf.get("max_data_level", _maxDataLevel);
    conf.get("min_level", _minLevel);
    //conf.get("min_valid_value", _minValidValue);
    conf.get("min_resolution", _minResolution);
    //conf.get("no_data_value", _noDataValue);
    //conf.get("profile", _profile);
    if (conf.hasChild("profile"))
        _profile = Profile::create(conf.child("profile"));
    conf.get("tile_size", _tileSize);
    conf.get("upsample", _upsample);

    _writingRequested = false;
    _dataExtentsIndex = nullptr;

    setProfile(Profile::create(Profile::GLOBAL_GEODETIC));
}

Config
TileLayer::getConfig() const
{
    auto conf = VisibleLayer::getConfig();
    conf.set("max_level", _maxLevel);
    conf.set("max_resolution", _maxResolution);
    //conf.get("max_valid_value", _maxValidValue);
    conf.set("max_data_level", _maxDataLevel);
    conf.set("min_level", _minLevel);
    //conf.get("min_valid_value", _minValidValue);
    conf.set("min_resolution", _minResolution);
    //conf.get("no_data_value", _noDataValue);

    if (_profile)
        conf.set("profile", _profile->getConfig());

    conf.set("tile_size", _tileSize);
    conf.set("upsample", _upsample);
    return conf;
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
void TileLayer::setUpsample(bool value) {
    _upsample = true;
}
const optional<bool>& TileLayer::upsample() const {
    return _upsample;
}

Status
TileLayer::openImplementation(const IOOptions& io)
{
    Status parent = VisibleLayer::openImplementation(io);
    if (parent.failed())
        return parent;

#if 0
    // If the user asked for a custom profile, install it now
    if (options().profile().has_value())
        setProfile(Profile::create(options().profile().get()));
#endif

#if 0
    if (isOpen())
        _cacheBinMetadata.clear();

    if (_memCache.valid())
        _memCache->clear();
#endif

    return getStatus();
}

Status
TileLayer::closeImplementation()
{
    return Layer::closeImplementation();
}

void
TileLayer::addedToMap(const Map* map)
{
    VisibleLayer::addedToMap(map);

    unsigned l2CacheSize = 0u;

    // If the profiles don't match, mosaicing will be likely so set up a
    // small L2 cache for this layer.
    if (map &&
        map->getProfile() &&
        getProfile() &&
        !map->getProfile()->getSRS()->isHorizEquivalentTo(getProfile()->getSRS()))
    {
        l2CacheSize = 16u;
        ROCKY_INFO << LC << "Map/Layer profiles differ; requesting L2 cache" << std::endl;
    }

    // Use the user defined option if it's set.
    if (_l2cachesize.has_value())
    {
        l2CacheSize = _l2cachesize.value();
    }

    setUpL2Cache(l2CacheSize);
}

void
TileLayer::removedFromMap(const Map* map)
{
    VisibleLayer::removedFromMap(map);
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
        ROCKY_INFO << LC << "L2 cache size set from environment = " << l2CacheSize << "\n";
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
        ROCKY_INFO << LC << "L2 cache size = " << l2CacheSize << std::endl;
    }
}

const Status&
TileLayer::openForWriting()
{
    if (isWritingSupported())
    {
        _writingRequested = true;
        open();
        return getStatus();
    }
    return setStatus(Status::ServiceUnavailable, "Layer does not support writing");
}

void
TileLayer::establishCacheSettings()
{
    //nop
}

shared_ptr<Profile>
TileLayer::getProfile() const
{
    return _profile;
}

void
TileLayer::setProfile(shared_ptr<Profile> profile)
{
    _profile = profile;

    if (getProfile())
    {
        // augment the final profile with any overrides:
        applyProfileOverrides(_profile);

        //ROCKY_INFO << LC
        //    << (getProfile()? getProfile()->toString() : "[no profile]") << " "
        //    //<< (getCacheSettings()? getCacheSettings()->toString() : "[no cache settings]")
        //    << std::endl;
    }
}

bool
TileLayer::isDynamic() const
{
    if (getHints().dynamic().isSetTo(true))
        return true;
    else
        return false;
}

std::string
TileLayer::getMetadataKey(const Profile* profile) const
{
    if (profile)
        return Stringify() << std::hex << profile->getHorizSignature() << "_metadata";
    else
        return "_metadata";
}

#if 0
CacheBin*
TileLayer::getCacheBin(const Profile* profile)
{
    if ( !isOpen() )
    {
        ROCKY_WARN << LC << "Illegal- called getCacheBin() before layer is open.. did you call open()?\n";
        return 0L;
    }

    CacheSettings* cacheSettings = getCacheSettings();
    if (!cacheSettings)
        return 0L;

    if (cacheSettings->cachePolicy()->isCacheDisabled())
        return 0L;

    CacheBin* bin = cacheSettings->getCacheBin();
    if (!bin)
        return 0L;

    // does the metadata need initializing?
    std::string metaKey = getMetadataKey(profile);    

    // See if the cache bin metadata is already stored.
    {
        ScopedReadLock lock(_data_mutex);
        if (_cacheBinMetadata.find(metaKey) != _cacheBinMetadata.end())
            return bin;
    }

    // We need to update the cache bin metadata
    ScopedWriteLock lock(_data_mutex);

    // read the metadata record from the cache bin:
    ReadResult rr = bin->readString(metaKey, getReadOptions());

    osg::ref_ptr<CacheBinMetadata> meta;
    bool metadataOK = false;

    if (rr.succeeded())
    {
        // Try to parse the metadata record:
        Config conf;
        conf.fromJSON(rr.getString());
        meta = new CacheBinMetadata(conf);

        if (meta->ok())
        {
            metadataOK = true;

            if (cacheSettings->cachePolicy()->isCacheOnly() && !_profile.valid())
            {
                // in cacheonly mode, create a profile from the first cache bin accessed
                // (they SHOULD all be the same...)
                setProfile(Profile::create(meta->_sourceProfile.get()));
                options().tileSize().init(meta->_sourceTileSize.get());
            }

            bin->setMetadata(meta.get());
        }
        else
        {
            ROCKY_WARN << LC << "Metadata appears to be corrupt.\n";
        }
    }

    if (!metadataOK)
    {
        // cache metadata does not exist, so try to create it.
        if (getProfile())
        {
            meta = new CacheBinMetadata();

            // no existing metadata; create some.
            meta->_cacheBinId = _runtimeCacheId;
            meta->_sourceName = this->getName();
            meta->_sourceTileSize = getTileSize();
            meta->_sourceProfile = getProfile()->toProfileOptions();
            meta->_cacheProfile = profile->toProfileOptions();
            meta->_cacheCreateTime = DateTime().asTimeStamp();
            // Use _dataExtents directly here since the _data_mutex is already locked.
            meta->_dataExtents = _dataExtents;

            // store it in the cache bin.
            std::string data = meta->getConfig().toJSON(false);
            osg::ref_ptr<StringObject> temp = new StringObject(data);
            bin->write(metaKey, temp.get(), getReadOptions());

            bin->setMetadata(meta.get());
        }

        else if (cacheSettings->cachePolicy()->isCacheOnly())
        {
            disable(Stringify() <<
                "Failed to open a cache for layer "
                "because cache_only policy is in effect and bin [" << _runtimeCacheId << "] "
                "could not be located.");

            return 0L;
        }

        else
        {
            ROCKY_WARN << LC <<
                "Failed to create cache bin [" << _runtimeCacheId << "] "
                "because there is no valid profile."
                << std::endl;

            cacheSettings->cachePolicy() = CachePolicy::NO_CACHE;
            return 0L;
        }
    }

    // If we loaded a profile from the cache metadata, apply the overrides:
    applyProfileOverrides(_profile);

    if (meta.valid())
    {
        _cacheBinMetadata[metaKey] = meta.get();
        ROCKY_DEBUG << LC << "Established metadata for cache bin [" << _runtimeCacheId << "]" << std::endl;
    }

    return bin;
}
#endif

void
TileLayer::disable(const std::string& msg)
{
    setStatus(Status::Error(msg));
}

#if 0
TileLayer::CacheBinMetadata*
TileLayer::getCacheBinMetadata(const Profile* profile)
{
    if (!profile)
        return nullptr;

    ScopedReadLock lock(_data_mutex);
    auto i = _cacheBinMetadata.find(getMetadataKey(profile));
    return i != _cacheBinMetadata.end() ? i->second.get() : nullptr;
}
#endif

bool
TileLayer::isKeyInLegalRange(const TileKey& key) const
{
    if ( !key.valid() )
    {
        return false;
    }

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = getProfile() ?
        getProfile()->getEquivalentLOD(key.getProfile(), key.getLOD()) :
        key.getLOD();


    // First check the key against the min/max level limits, it they are set.
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    //if ((options().maxLevel().has_value() && localLOD > options().maxLevel().value()) ||
    //    (options().minLevel().has_value() && localLOD < options().minLevel().value()))
    {
        return false;
    }

    // Next check the maxDataLevel if that is set.
    if (_maxDataLevel.has_value() && localLOD > _maxDataLevel)
    //if (options().maxDataLevel().has_value() && localLOD > options().maxDataLevel().get())
    {
        return false;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    //if (options().minResolution().has_value() || options().maxResolution().has_value())
    {
        if (getProfile())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.getExtent().width() / (double)tileSize();
            double resLayer = key.getProfile()->getSRS()->transformUnits(resKey, getProfile()->getSRS());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            //if (options().maxResolution().has_value() &&
            //    options().maxResolution().value() > resLayer)
            {
                return false;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            //if (options().minResolution().has_value() &&
            //    options().minResolution().value() < resLayer)
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
    unsigned localLOD = getProfile() ?
        getProfile()->getEquivalentLOD(key.getProfile(), key.getLOD()) :
        key.getLOD();


    // First check the key against the min/max level limits, it they are set.
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    //if ((options().maxLevel().has_value() && localLOD > options().maxLevel().value()) ||
    //    (options().minLevel().has_value() && localLOD < options().minLevel().value()))
    {
        return false;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (getProfile())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.getExtent().width() / (double)tileSize();
            double resLayer = key.getProfile()->getSRS()->transformUnits(resKey, getProfile()->getSRS());

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

#if 0
bool
TileLayer::isCached(const TileKey& key) const
{
    // first consult the policy:
    if (getCacheSettings()->isCacheDisabled())
        return false;

    else if (getCacheSettings()->cachePolicy()->isCacheOnly())
        return true;

    // next check for a bin:
    CacheBin* bin = const_cast<TileLayer*>(this)->getCacheBin( key.getProfile() );
    if ( !bin )
        return false;

    return bin->getRecordStatus( key.str() ) == CacheBin::STATUS_OK;
}
#endif

unsigned int TileLayer::getDataExtentsSize() const
{
    ScopedReadLock lk(_data_mutex);
    return _dataExtents.size();
}

void TileLayer::getDataExtents(DataExtentList& dataExtents) const
{
    ScopedReadLock lk(_data_mutex);
    if (!_dataExtents.empty())
    {
        dataExtents = _dataExtents;
    }

#if 0
    else if (!_cacheBinMetadata.empty())
    {
        // There are extents in the cache bin, so use those.
        // The DE's are the same regardless of profile so just use the first one in there.
        dataExtents = _cacheBinMetadata.begin()->second->_dataExtents;
    }
#endif
}

void TileLayer::setDataExtents(const DataExtentList& dataExtents)
{
    ScopedWriteLock lk(_data_mutex);
    _dataExtents = dataExtents;
    dirtyDataExtents();
}

void TileLayer::addDataExtent(const DataExtent& dataExtent)
{
    ScopedWriteLock lk(_data_mutex);
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
TileLayer::getDataExtentsUnion() const
{
    if (!_dataExtentsUnion.valid() && getDataExtentsSize() > 0)
    {
        ScopedWriteLock lock(_data_mutex);
        {
            if (!_dataExtentsUnion.valid() && _dataExtents.size() > 0) // double-check
            {
                _dataExtentsUnion = _dataExtents[0];
                for (unsigned int i = 1; i < _dataExtents.size(); i++)
                {
                    _dataExtentsUnion.expandToInclude(_dataExtents[i]);

                    if (_dataExtents[i].minLevel().has_value())
                        _dataExtentsUnion.minLevel() = std::min(_dataExtentsUnion.minLevel().get(), _dataExtents[i].minLevel().get());

                    if (_dataExtents[i].maxLevel().has_value())
                        _dataExtentsUnion.maxLevel() = std::max(_dataExtentsUnion.maxLevel().get(), _dataExtents[i].maxLevel().get());
                }

                // if upsampling is enabled include the MDL in the union.
                if (_maxDataLevel.has_value() && upsample())
                {
                    _dataExtentsUnion.maxLevel() = std::max(_dataExtentsUnion.maxLevel().get(), _maxDataLevel.value());
                }
            }
        }
    }
    return _dataExtentsUnion;
}

const GeoExtent&
TileLayer::getExtent() const
{
    return getDataExtentsUnion();
}

TileKey
TileLayer::getBestAvailableTileKey(
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
    unsigned localLOD = getProfile() ?
        getProfile()->getEquivalentLOD(key.getProfile(), key.getLOD()) :
        key.getLOD();

    // Check against level extrema:
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return TileKey::INVALID;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (getProfile())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.getExtent().width() / (double)tileSize();
            double resLayer = key.getProfile()->getSRS()->transformUnits(resKey, getProfile()->getSRS());

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
    if (getDataExtentsSize() == 0)
    {
        return localLOD > MDL ? key.createAncestorKey(MDL) : key;
    }

    // Reject if the extents don't overlap at all.
    // (Note: this does not consider min/max levels, only spatial extents)
    if (!getDataExtentsUnion().intersects(key.getExtent()))
    {
        return TileKey::INVALID;
    }

    bool     intersects = false;
    unsigned highestLOD = 0;

    double a_min[2], a_max[2];
    // Build the index if needed.
    if (!_dataExtentsIndex)
    {
        ScopedWriteLock lock(_data_mutex);
        if (!_dataExtentsIndex) // Double check
        {
            ROCKY_INFO << LC << "Building data extents index with " << _dataExtents.size() << " extents" << std::endl;
            DataExtentsIndex* dataExtentsIndex = new DataExtentsIndex();
            for (auto de = _dataExtents.begin(); de != _dataExtents.end(); ++de)
            {
                // Build the index in the SRS of this layer
                GeoExtent extentInLayerSRS = getProfile()->clampAndTransformExtent(*de);

                if (extentInLayerSRS.getSRS()->isGeographic() && extentInLayerSRS.crossesAntimeridian())
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
    GeoExtent keyExtentInLayerSRS = getProfile()->clampAndTransformExtent(key.getExtent());

    a_min[0] = keyExtentInLayerSRS.xMin(); a_min[1] = keyExtentInLayerSRS.yMin();
    a_max[0] = keyExtentInLayerSRS.xMax(); a_max[1] = keyExtentInLayerSRS.yMax();

    DataExtentsIndex* index = static_cast<DataExtentsIndex*>(_dataExtentsIndex);

    TileKey bestKey;
    index->Search(a_min, a_max, [&](const DataExtent& de) {
        // check that the extent isn't higher-resolution than our key:
        if (!de.minLevel().has_value() || localLOD >= (int)de.minLevel().get())
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
            else if (localLOD <= (int)de.maxLevel().get())
            {
                bestKey = localLOD > MDL ? key.createAncestorKey(MDL) : key;
                return false; //Stop searching, we've found a key
            }

            // otherwise, record the highest encountered LOD that
            // intersects our key.
            else if (de.maxLevel().get() > highestLOD)
            {
                highestLOD = de.maxLevel().get();
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
        if (considerUpsampling && (upsample() == true))
        {
            // for a normal dataset, MDL takes priority.
            unsigned maxAvailableLOD = std::max(highestLOD, MDL);
            return key.createAncestorKey(std::min(key.getLOD(), maxAvailableLOD));
        }
        else
        {
            // for a normal dataset, dataset max takes priority over MDL.
            unsigned maxAvailableLOD = std::min(highestLOD, MDL);
            return key.createAncestorKey(std::min(key.getLOD(), maxAvailableLOD));
        }
    }

    return TileKey::INVALID;
}

bool
TileLayer::mayHaveData(const TileKey& key) const
{
    return
        key == getBestAvailableTileKey(key, true);
}
