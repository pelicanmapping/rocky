/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ElevationLayer.h"
#include "Geoid.h"
#include "Heightfield.h"
#include "Metrics.h"
#include "VerticalDatum.h"

#include <cinttypes>

using namespace rocky;
using namespace rocky::util;

#define LC "[ElevationLayer] \"" << name().value() << "\" : "

//------------------------------------------------------------------------

#if 0
Config
ElevationLayer::Options::getConfig() const
{
    Config conf = TileLayer::Options::getConfig();
    conf.set("vdatum", verticalDatum());
    conf.set("offset", offset());
    conf.set("nodata_policy", "default", _noDataPolicy, NODATA_INTERPOLATE);
    conf.set("nodata_policy", "interpolate", _noDataPolicy, NODATA_INTERPOLATE);
    conf.set("nodata_policy", "msl", _noDataPolicy, NODATA_MSL);
    return conf;
}

void
ElevationLayer::Options::fromConfig(const Config& conf)
{
    _offset.init(false);
    _noDataPolicy.init(NODATA_INTERPOLATE);

    conf.get("vdatum", verticalDatum());
    conf.get("vsrs", verticalDatum());    // back compat
    conf.get("offset", offset());
    conf.get("nodata_policy", "default", _noDataPolicy, NODATA_INTERPOLATE);
    conf.get("nodata_policy", "interpolate", _noDataPolicy, NODATA_INTERPOLATE);
    conf.get("nodata_policy", "msl", _noDataPolicy, NODATA_MSL);

    // ElevationLayers are special in that visible really maps to whether the layer is open or closed
    // If a layer is marked as enabled (openAutomatically) but also marked as visible=false set
    // openAutomatically to false to prevent the layer from being opened at startup.
    // This prevents a deadlock b/c setVisible is called during the VisibleLayer openImplementation and it
    // will call setVisible on the Layer there which will result in trying to close the layer while it's opening.
    // There may be a better way to sync these up but will require a bit of rework.
    if (openAutomatically() == true && visible() == false)
    {
        openAutomatically() = false;
    }
}
#endif
//------------------------------------------------------------------------

namespace
{
    // perform very basic sanity-check validation on a heightfield.
    bool validateHeightfield(const Heightfield* hf)
    {
        if (!hf)
            return false;
        if (hf->height() < 1 || hf->height() > 1024) {
            ROCKY_WARN << "row count = " << hf->height() << std::endl;
            return false;
        }
        if (hf->width() < 1 || hf->width() > 1024) {
            ROCKY_WARN << "col count = " << hf->width() << std::endl;
            return false;
        }
        //if (hf->getHeightList().size() != hf->width() * hf->height()) {
        //    OE_WARN << "mismatched data size" << std::endl;
        //    return false;
        //}
        //if (hf->getXInterval() < 1e-5 || hf->getYInterval() < 1e-5)
        //    return false;

        return true;
    }
}

//------------------------------------------------------------------------

ElevationLayer::ElevationLayer() :
    super()
{
    construct(Config());
}

ElevationLayer::ElevationLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
ElevationLayer::construct(const Config& conf)
{
    _offset.setDefault(false);
    _noDataValue.setDefault(NO_DATA_VALUE);
    _minValidValue.setDefault(-FLT_MAX);
    _maxValidValue.setDefault(FLT_MAX);
//    _noDataPolicy.setDefault(NODATA_INTERPOLATE);

    conf.get("vdatum", _verticalDatum);
    conf.get("offset", _offset);
    conf.get("nodata_value", _noDataValue);
    conf.get("no_data_value", _noDataValue);
    conf.get("min_valid_Value", _minValidValue);
    conf.get("max_valid_value", _maxValidValue);

    //conf.get("nodata_policy", "default", _noDataPolicy, NODATA_INTERPOLATE);
    //conf.get("nodata_policy", "interpolate", _noDataPolicy, NODATA_INTERPOLATE);
    //conf.get("nodata_policy", "msl", _noDataPolicy, NODATA_MSL);

    // ElevationLayers are special in that visible really maps to whether the layer is open or closed
    // If a layer is marked as enabled (openAutomatically) but also marked as visible=false set
    // openAutomatically to false to prevent the layer from being opened at startup.
    // This prevents a deadlock b/c setVisible is called during the VisibleLayer openImplementation and it
    // will call setVisible on the Layer there which will result in trying to close the layer while it's opening.
    // There may be a better way to sync these up but will require a bit of rework.
    if (getOpenAutomatically() == true && getVisible() == false)
    {
        _openAutomatically = false;
    }

    _sentry.setName("ElevationLayer " + name().value());

    // override with a different default tile size since elevation
    // tiles need overlapping edges
    if (!_tileSize.has_value())
    {
        _tileSize.setDefault(257u);
    }

    // a small L2 cache will help with things like normal map creation
    // (i.e. queries that sample neighboring tiles)
    if (!_l2cachesize.has_value())
    {
        _l2cachesize = 4u;
    }

    // Disable max-level support for elevation data because it makes no sense.
    _maxLevel.clear();
    _maxResolution.clear();
    //options().maxLevel().clear();
    //options().maxResolution().clear();

    // elevation layers do not render directly; rather, a composite of elevation data
    // feeds the terrain engine to permute the mesh.
    //setRenderType(RENDERTYPE_NONE);
}

Config
ElevationLayer::getConfig() const
{
    Config conf = TileLayer::getConfig();

    conf.set("vdatum", _verticalDatum);
    conf.set("offset", _offset);
    conf.set("no_data_value", _noDataValue);
    conf.set("min_valid_Value", _minValidValue);
    conf.set("max_valid_value", _maxValidValue);

    //conf.set("nodata_policy", "default", _noDataPolicy, NODATA_INTERPOLATE);
    //conf.set("nodata_policy", "interpolate", _noDataPolicy, NODATA_INTERPOLATE);
    //conf.set("nodata_policy", "msl", _noDataPolicy, NODATA_MSL);

    return conf;
}

void
ElevationLayer::setVisible(bool value)
{
    VisibleLayer::setVisible(value);
    if (value)
        open();
    else
        close();
}

void ElevationLayer::setVerticalDatum(const std::string& value) {
    _verticalDatum = value, _reopenRequired = true;
    // vertical datum change requires a profile override:
    applyProfileOverrides(_profile);
}
const optional<std::string>& ElevationLayer::verticalDatum() const {
    return _verticalDatum;
}
void ElevationLayer::setOffset(bool value) {
    _offset = value, _reopenRequired = true;
}
const optional<bool>& ElevationLayer::offset() const {
    return _offset;
}
void ElevationLayer::setNoDataValue(float value) {
    _noDataValue = value, _reopenRequired = true;
}
const optional<float>& ElevationLayer::noDataValue() const {
    return _noDataValue;
}
void ElevationLayer::setMinValidValue(float value) {
    _minValidValue = value, _reopenRequired = true;
}
const optional<float>& ElevationLayer::minValidValue() const {
    return _minValidValue;
}

void ElevationLayer::setMaxValidValue(float value) {
    _maxValidValue = value, _reopenRequired = true;
}
const optional<float>& ElevationLayer::maxValidValue() const {
    return _maxValidValue;
}


//void
//ElevationLayer::setNoDataPolicy(const ElevationNoDataPolicy& value) {
//    _noDataPolicy = value, _reopenRequired = true;
//}
//const ElevationNoDataPolicy&
//ElevationLayer::getNoDataPolicy() const {
//    return _noDataPolicy;
//}

void
ElevationLayer::normalizeNoDataValues(Heightfield* hf) const
{
    if ( hf )
    {
        // we know heightfields are R32_SFLOAT so take a shortcut.
        float* pixel = hf->data<float>();
        for (unsigned i = 0; i < hf->width()*hf->height(); ++i, ++pixel)
        {
            float h = *pixel;
            if (std::isnan(h) ||
                equivalent(h, noDataValue().value()) ||
                h < minValidValue() ||
                h > maxValidValue())
            {
                *pixel = NO_DATA_VALUE;
            }
        }
    }
}

void
ElevationLayer::applyProfileOverrides(
    shared_ptr<Profile>& inOutProfile) const
{
    // Check for a vertical datum override.
    if (inOutProfile && inOutProfile->valid() && _verticalDatum.isSet())
    {
        std::string vdatum = _verticalDatum;

        std::string profileVDatumStr = _profile->getSRS()->getVertInitString();
        if (profileVDatumStr.empty())
            profileVDatumStr = "geodetic";

        ROCKY_INFO << LC << "Override vdatum = " << vdatum << " (was " << profileVDatumStr << ")" << std::endl;

        if (!util::ciEquals(getProfile()->getSRS()->getVertInitString(), vdatum))
        {
            Config conf = getConfig();
            conf.set("vdatum", vdatum);
            inOutProfile = Profile::create(conf);
        }
    }
}

void
ElevationLayer::assembleHeightfield(
    const TileKey& key,
    shared_ptr<Heightfield> out_hf,
    IOControl* ioc) const
{
    // Collect the heightfields for each of the intersecting tiles.
    std::vector<GeoHeightfield> geohf_list;

    // Determine the intersecting keys
    std::vector<TileKey> intersectingKeys;

    if (key.getLOD() > 0u)
    {
        key.getIntersectingKeys(getProfile(), intersectingKeys);
        //getProfile()->getIntersectingTiles(key, intersectingTiles);
    }

    else
    {
        // LOD is zero - check whether the LOD mapping went out of range, and if so,
        // fall back until we get valid tiles. This can happen when you have two
        // profiles with very different tile schemes, and the "equivalent LOD"
        // surpasses the max data LOD of the tile source.
        unsigned numTilesThatMayHaveData = 0u;

        int intersectionLOD = getProfile()->getEquivalentLOD(key.getProfile(), key.getLOD());

        while (numTilesThatMayHaveData == 0u && intersectionLOD >= 0)
        {
            intersectingKeys.clear();

            TileKey::getIntersectingKeys(
                key.getExtent(),
                intersectionLOD,
                getProfile(),
                intersectingKeys);
            //getProfile()->getIntersectingTiles(key.getExtent(), intersectionLOD, intersectingTiles);

            for (auto& layerKey : intersectingKeys)
            {
                if (mayHaveData(layerKey))
                {
                    ++numTilesThatMayHaveData;
                }
            }

            --intersectionLOD;
        }
    }

    // collect heightfield for each intersecting key. Note, we're hitting the
    // underlying tile source here, so there's no vetical datum shifts happening yet.
    // we will do that later.
    if (intersectingKeys.size() > 0)
    {
        for(auto& layerKey : intersectingKeys)
        {
            if ( isKeyInLegalRange(layerKey) )
            {
                util::ScopedReadLock lock(layerMutex());

                auto result = createHeightfieldImplementation(layerKey, ioc);

                if (result.status.ok() && result.value.valid())
                {
                    geohf_list.push_back(result.value);
                }
            }
        }

        // If we actually got a Heightfield, resample/reproject it to match the incoming TileKey's extents.
        if (geohf_list.size() > 0)
        {
            unsigned width = 0;
            unsigned height = 0;

            for(auto& geohf : geohf_list)
            {
                width = std::max(width, geohf.getHeightfield()->width());
                height = std::max(height, geohf.getHeightfield()->height());
            }

            // Now sort the heightfields by resolution to make sure we're sampling
            // the highest resolution one first.
            std::sort(geohf_list.begin(), geohf_list.end(), GeoHeightfield::SortByResolutionFunctor());

            // new output HF:
            out_hf = Heightfield::create(width, height);

            //Go ahead and set up the heightfield so we don't have to worry about it later
            double minx, miny, maxx, maxy;
            key.getExtent().getBounds(minx, miny, maxx, maxy);
            double dx = (maxx - minx)/(double)(width-1);
            double dy = (maxy - miny)/(double)(height-1);

            //Create the new heightfield by sampling all of them.
            for (unsigned int c = 0; c < width; ++c)
            {
                double x = minx + (dx * (double)c);
                for (unsigned r = 0; r < height; ++r)
                {
                    double y = miny + (dy * (double)r);

                    //For each sample point, try each heightfield.  The first one with a valid elevation wins.
                    float elevation = NO_DATA_VALUE;
                    fvec3 normal(0,0,1);

                    for (auto& geohf : geohf_list)
                    {
                        // get the elevation value, at the same time transforming it vertically into the
                        // requesting key's vertical datum.
                        float e = 0.0;

                        if (geohf.getElevation(
                            key.getExtent().getSRS(),
                            x, y,
                            Heightfield::BILINEAR,
                            key.getExtent().getSRS(),
                            e))
                        {
                            elevation = e;
                            break;
                        }
                    }

                    out_hf->write(dvec4(elevation), c, r);
                }
            }
        }
        else
        {
            //if (progress && progress->message().empty())
            //    progress->message() = "assemble yielded no heightfields";
        }
    }
    else
    {
        //if (progress && progress->message().empty())
        //    progress->message() = "assemble yielded no intersecting tiles";
    }


    // If the progress was cancelled clear out any of the output data.
    if (ioc && ioc->isCanceled())
    {
        out_hf = nullptr;
    }
}

Result<GeoHeightfield>
ElevationLayer::createHeightfield(const TileKey& key)
{
    return createHeightfield(key, nullptr);
}

Result<GeoHeightfield>
ElevationLayer::createHeightfield(
    const TileKey& key,
    IOControl* ioc)
{    
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT(getName());
    ROCKY_PROFILING_ZONE_TEXT(key.str());

    // If the layer is disabled, bail out
    if (!isOpen())
    {
        return Result(GeoHeightfield::INVALID);
    }

    //NetworkMonitor::ScopedRequestLayer layerRequest(getName());

    return createHeightfieldInKeyProfile(key, ioc);
}

Result<GeoHeightfield>
ElevationLayer::createHeightfieldInKeyProfile(
    const TileKey& key,
    IOControl* ioc)
{
    GeoHeightfield result;
    shared_ptr<Heightfield> hf;

    auto profile = getProfile();
    if (!profile || !isOpen())
    {
        return Result<GeoHeightfield>(Status::ResourceUnavailable, "Layer not open or initialize");
    }

    ROCKY_TODO("cache, gate");

#if 0
    // Prevents more than one thread from creating the same object
    // at the same time. This helps a lot with elevation data since
    // the many queries cross tile boundaries (like calculating 
    // normal maps)
    ScopedGate<TileKey> gate(_sentry, key, [&]() {
        return _memCache.valid();
    });

    // Check the memory cache first
    bool fromMemCache = false;

    // cache key combines the key with the full signature (incl vdatum)
    // the cache key combines the Key and the horizontal profile.
    std::string cacheKey = Cache::makeCacheKey(
        Stringify() << key.str() << "-" << std::hex << key.getProfile()->getHorizSignature(),
        "elevation");
    const CachePolicy& policy = getCacheSettings()->cachePolicy().get();

    char memCacheKey[64];

    // Try the L2 memory cache first:
    if ( _memCache.valid() )
    {
        sprintf(memCacheKey, "%d/%s/%s",
            getRevision(),
            key.str().c_str(),
            key.getProfile()->getHorizSignature().c_str());

        CacheBin* bin = _memCache->getOrCreateDefaultBin();
        ReadResult cacheResult = bin->readObject(memCacheKey, 0L);
        if ( cacheResult.succeeded() )
        {
            result = GeoHeightfield(
                static_cast<osg::Heightfield*>(cacheResult.releaseObject()),
                key.getExtent());

            fromMemCache = true;
        }
    }
#endif
    // Next try the main cache:
    if ( !result.valid() )
    {
#if 0
        // See if there's a persistent cache.
        CacheBin* cacheBin = getCacheBin( key.getProfile() );

        // validate the existance of a valid layer profile.
        if ( !policy.isCacheOnly() && !getProfile() )
        {
            disable("Could not establish a valid profile.. did you set one?");
            return GeoHeightfield::INVALID;
        }

        // Now attempt to read from the cache. Since the cached data is stored in the
        // map profile, we can try this first.
        bool fromCache = false;

        osg::ref_ptr< osg::Heightfield > cachedHF;

        if ( cacheBin && policy.isCacheReadable() )
        {
            ReadResult r = cacheBin->readObject(cacheKey, 0L);
            if ( r.succeeded() )
            {
                bool expired = policy.isExpired(r.lastModifiedTime());
                cachedHF = r.get<osg::Heightfield>();
                if ( cachedHF && validateHeightfield(cachedHF.get()) )
                {
                    if (!expired)
                    {
                        hf = cachedHF;
                        fromCache = true;
                    }
                }
            }
        }

        // if we're cache-only, but didn't get data from the cache, fail silently.
        if ( !hf.valid() && policy.isCacheOnly() )
        {
            return GeoHeightfield::INVALID;
        }
#endif
        // If we didn't get anything from cache, time to create one:
        if ( !hf )
        {
            // Check that the key is legal (in valid LOD range, etc.)
            if ( !isKeyInLegalRange(key) )
            {
                return Result(GeoHeightfield::INVALID);
            }

            if (key.getProfile()->isHorizEquivalentTo(profile))
            {
                util::ScopedReadLock lock(layerMutex());
                auto r = createHeightfieldImplementation(key, ioc);
                if (r.status.failed())
                    return r;
                else
                    result = r.value;
            }
            else
            {
                // If the profiles are different, use a compositing method to assemble the tile.
                shared_ptr<Heightfield> hf;
                assembleHeightfield(key, hf, ioc);
                result = GeoHeightfield(hf, key.getExtent());
            }

            // Check for cancelation before writing to a cache
            if (ioc && ioc->isCanceled())
            {
                return Result(GeoHeightfield::INVALID);
            }

            // The const_cast is safe here because we just created the
            // heightfield from scratch...not from a cache.
            hf = result.getHeightfield();
            //hf = const_cast<osg::Heightfield*>(result.getHeightfield());

            // validate it to make sure it's legal.
            if (hf && !validateHeightfield(hf.get()))
            {
                ROCKY_WARN << LC << "Generated an illegal heightfield!" << std::endl;
                return Result<GeoHeightfield>(Status::GeneralError, "Generated an illegal heightfield!");
                //hf = 0L; // to fall back on cached data if possible.
            }

            // Pre-caching operations:
            normalizeNoDataValues(hf.get());

            // If the result is good, we now have a heightfield but its vertical values
            // are still relative to the source's vertical datum. Convert them.
            if (hf && !key.getExtent().getSRS()->isVertEquivalentTo(profile->getSRS().get()))
            {
                VerticalDatum::transform(
                    profile->getSRS()->getVerticalDatum().get(),    // from
                    key.getExtent().getSRS()->getVerticalDatum().get(),  // to
                    key.getExtent(),
                    hf);
            }

#if 0
            // Invoke user callbacks
            if (result.valid())
            {
                invoke_onCreate(key, result);
            }

            // If we have a cacheable heightfield, and it didn't come from the cache
            // itself, cache it now.
            if ( hf.valid()    &&
                 cacheBin      &&
                 !fromCache    &&
                 policy.isCacheWriteable() )
            {
                OE_PROFILING_ZONE_NAMED("cache write");
                cacheBin->write(cacheKey, hf.get(), 0L);
            }

            // If we have an expired heightfield from the cache and were not able to create
            // any new data, just return the cached data.
            if (!hf.valid() && cachedHF.valid())
            {
                OE_DEBUG << LC << "Using cached but expired heightfield for " << key.str() << std::endl;
                hf = cachedHF;
            }
#endif

            // No luck on any path:
            if (hf == nullptr)
            {
                return Result(GeoHeightfield::INVALID);
            }
        }

        if (hf)
        {
            result = GeoHeightfield(hf, key.getExtent());
        }
    }

    // Check for cancelation before writing to a cache:
    if (ioc && ioc->isCanceled())
    {
        return Result(GeoHeightfield::INVALID);
    }

#if 0
    // write to mem cache if needed:
    if ( result.valid() && !fromMemCache && _memCache.valid() )
    {
        CacheBin* bin = _memCache->getOrCreateDefaultBin();
        bin->write(memCacheKey, result.getHeightfield(), 0L);
    }
#endif

    return Result(result);
}

Status
ElevationLayer::writeHeightfield(
    const TileKey& key,
    const Heightfield* hf,
    IOControl* ioc) const
{
    if (isWritingSupported() && isWritingRequested())
    {
        util::ScopedReadLock lock(layerMutex());
        return writeHeightfieldImplementation(key, hf, ioc);
    }
    return Status::ServiceUnavailable;
}

Status
ElevationLayer::writeHeightfieldImplementation(
    const TileKey& key,
    const Heightfield* hf,
    IOControl* ioc) const
{
    return Status::ServiceUnavailable;
}

#if 0
void
ElevationLayer::invoke_onCreate(const TileKey& key, GeoHeightfield& data)
{
    if (_callbacks.empty() == false) // not thread-safe but that's ok
    {
        // Copy the vector to prevent thread lockup
        Callbacks temp;

        _callbacks.lock();
        temp = _callbacks;
        _callbacks.unlock();

        for(Callbacks::const_iterator i = temp.begin();
            i != temp.end();
            ++i)
        {
            i->get()->onCreate(key, data);
        }
    }
}

void
ElevationLayer::addCallback(ElevationLayer::Callback* c)
{
    _callbacks.lock();
    _callbacks.push_back(c);
    _callbacks.unlock();
}

void
ElevationLayer::removeCallback(ElevationLayer::Callback* c)
{
    _callbacks.lock();
    Callbacks::iterator i = std::find(_callbacks.begin(), _callbacks.end(), c);
    if (i != _callbacks.end())
        _callbacks.erase(i);
    _callbacks.unlock();
}
#endif

//------------------------------------------------------------------------

#undef  LC
#define LC "[ElevationLayers] "

namespace
{
    struct LayerData
    {
        shared_ptr<ElevationLayer> layer;
        TileKey key;
        bool isFallback;
        int index;
    };

    using LayerDataVector = std::vector<LayerData>;

    void resolveInvalidHeights(
        Heightfield* grid,
        const GeoExtent&  ex,
        float invalidValue,
        const Geoid* geoid)
    {
        if (!grid)
            return;

        if (geoid)
        {
            // need the lat/long extent for geoid queries:
            unsigned numRows = grid->height();
            unsigned numCols = grid->width();
            GeoExtent geodeticExtent = 
                ex.getSRS()->isGeographic() ? ex :
                ex.transform(ex.getSRS()->getGeographicSRS());
            double latMin = geodeticExtent.yMin();
            double lonMin = geodeticExtent.xMin();
            double lonInterval = geodeticExtent.width() / (double)(numCols - 1);
            double latInterval = geodeticExtent.height() / (double)(numRows - 1);

            for (unsigned r = 0; r < numRows; ++r)
            {
                double lat = latMin + latInterval * (double)r;
                for (unsigned c = 0; c < numCols; ++c)
                {
                    double lon = lonMin + lonInterval * (double)c;
                    if (grid->heightAt(c, r) == invalidValue)
                    {
                        grid->heightAt(c, r) = geoid->getHeight(lat, lon);
                    }
                }
            }
        }
        else
        {
            grid->forEachHeight([invalidValue](float& height)
                {
                    if (height == invalidValue)
                        height = 0.0f;
                });
        }
    }

}

bool
ElevationLayerVector::populateHeightfield(
    shared_ptr<Heightfield> hf,
    std::vector<float>* resolutions,
    const TileKey& key,
    shared_ptr<Profile> haeProfile,
    Heightfield::Interpolation interpolation,
    IOControl* ioc) const
{
    // heightfield must already exist.
    if ( !hf )
        return false;

    ROCKY_PROFILING_ZONE;

    // if the caller provided an "HAE map profile", he wants an HAE elevation grid even if
    // the map profile has a vertical datum. This is the usual case when building the 3D
    // terrain, for example. Construct a temporary key that doesn't have the vertical
    // datum info and use that to query the elevation data.
    TileKey keyToUse = key;
    if (haeProfile)
    {
        keyToUse = TileKey(key.getLOD(), key.getTileX(), key.getTileY(), haeProfile);
    }

    // Collect the valid layers for this tile.
    LayerDataVector contenders;
    LayerDataVector offsets;

    int i;

    // Track the number of layers that would return fallback data.
    // if ALL layers would provide fallback data, we can exit early
    // and return nothing.
    unsigned numFallbackLayers = 0;

    // Check them in reverse order since the highest priority is last.
    for (i = size()-1; i>=0; --i)
    {
        auto layer = (*this)[i];

        if (layer->isOpen())
        {
            // calculate the resolution-mapped key (adjusted for tile resolution differential).
            TileKey mappedKey = keyToUse.mapResolution(
                hf->width(),
                layer->tileSize() );

            bool useLayer = true;
            TileKey bestKey( mappedKey );

            // Check whether the non-mapped key is valid according to the user's minLevel setting.
            // We wll ignore the maxDataLevel setting, because we account for that by getting
            // the "best available" key later. We must keep these layers around in case we need
            // to fill in empty spots.
            if (key.getLOD() < layer->minLevel())
            {
                useLayer = false;
            }

            // GW - this was wrong because it would exclude layers with a maxDataLevel set
            // below the requested LOD ... when in fact we need around for fallback.
            //if ( !layer->isKeyInLegalRange(key) )
            //{
            //    useLayer = false;
            //}

            // Find the "best available" mapped key from the tile source:
            else
            {
                bestKey = layer->getBestAvailableTileKey(mappedKey);
                if (bestKey.valid())
                {
                    // If the bestKey is not the mappedKey, this layer is providing
                    // fallback data (data at a lower resolution than requested)
                    if ( mappedKey != bestKey )
                    {
                        numFallbackLayers++;
                    }
                }
                else
                {
                    useLayer = false;
                }
            }

            if ( useLayer )
            {
                if ( layer->offset() == true)
                {
                    offsets.push_back(LayerData());
                    LayerData& ld = offsets.back();
                    ld.layer = layer;
                    ld.key = bestKey;
                    ld.isFallback = bestKey != mappedKey;
                    ld.index = i;
                }
                else
                {
                    contenders.push_back(LayerData());
                    LayerData& ld = contenders.back();
                    ld.layer = layer;
                    ld.key = bestKey;
                    ld.isFallback = bestKey != mappedKey;
                    ld.index = i;
                }

#ifdef ANALYZE
                layerAnalysis[layer].used = true;
#endif
            }
        }
    }

    // nothing? bail out.
    if ( contenders.empty() && offsets.empty() )
    {
        return false;
    }

    // if everything is fallback data, bail out.
    if ( contenders.size() + offsets.size() == numFallbackLayers )
    {
        return false;
    }

    // Sample the layers into our target.
    unsigned numColumns = hf->width();
    unsigned numRows = hf->height();
    double   xmin = key.getExtent().xMin();
    double   ymin = key.getExtent().yMin();
    double   dx = key.getExtent().width() / (double)(numColumns - 1);
    double   dy = key.getExtent().height() / (double)(numRows - 1);

    auto keySRS = keyToUse.getProfile()->getSRS();

    bool realData = false;

    unsigned int total = numColumns * numRows;

    int nodataCount = 0;

    TileKey actualKey; // Storage if a new key needs to be constructed

    bool requiresResample = true;

    // If we only have a single contender layer, and the tile is the same size as the requested
    // heightfield then we just use it directly and avoid having to resample it
    if (contenders.size() == 1 && offsets.empty())
    {
        ElevationLayer* layer = contenders[0].layer.get();

        auto layerHF = layer->createHeightfield(contenders[0].key, ioc);
        if (layerHF.value.valid())
        {
            if (layerHF.value.getHeightfield()->width() == hf->width() &&
                layerHF.value.getHeightfield()->height() == hf->height())
            {
                requiresResample = false;

                memcpy(
                    hf->data<unsigned char>(),
                    layerHF.value.getHeightfield()->data<unsigned char>(),
                    hf->sizeInBytes());
                //memcpy(hf->getFloatArray()->asVector().data(),
                //    layerHF.getHeightfield()->getFloatArray()->asVector().data(),
                //    sizeof(float) * hf->getFloatArray()->size()
                //);

                realData = true;

                if (resolutions)
                {
                    auto [resx, resy] = contenders[0].key.getResolution(hf->width());
                    for (unsigned i = 0; i < hf->width()*hf->height(); ++i)
                        (*resolutions)[i] = resy;
                }
            }
        }
    }

    // If we need to mosaic multiple layers or resample it to a new output tilesize go through a resampling loop.
    if (requiresResample)
    {
        // We will load the actual heightfields on demand. We might not need them all.
        std::vector<GeoHeightfield> heightfields(contenders.size());
        std::vector<TileKey> heightfieldActualKeys(contenders.size());
        std::vector<GeoHeightfield> offsetfields(offsets.size());
        std::vector<bool> heightFallback(contenders.size(), false);
        std::vector<bool> heightFailed(contenders.size(), false);
        std::vector<bool> offsetFailed(offsets.size(), false);

        // Initialize the actual keys to match the contender keys.
        // We'll adjust these as necessary if we need to fall back
        for(unsigned i=0; i<contenders.size(); ++i)
        {
            heightfieldActualKeys[i] = contenders[i].key;
        }

        // The maximum number of heightfields to keep in this local cache
        const unsigned maxHeightfields = 50;
        unsigned numHeightfieldsInCache = 0;

        for (unsigned c = 0; c < numColumns; ++c)
        {
            double x = xmin + (dx * (double)c);

            // periodically check for cancelation
            if (ioc && ioc->isCanceled())
            {
                return false;
            }

            for (unsigned r = 0; r < numRows; ++r)
            {
                double y = ymin + (dy * (double)r);

                // Collect elevations from each layer as necessary.
                int resolvedIndex = -1;

                float resolution = FLT_MAX;

                fvec3 normal_sum(0, 0, 0);

                for (int i = 0; i < contenders.size() && resolvedIndex < 0; ++i)
                {
                    ElevationLayer* layer = contenders[i].layer.get();
                    TileKey& contenderKey = contenders[i].key;
                    int index = contenders[i].index;

                    if (heightFailed[i])
                        continue;

                    GeoHeightfield& layerHF = heightfields[i];
                    TileKey& actualKey = heightfieldActualKeys[i];

                    if (!layerHF.valid())
                    {
                        // We couldn't get the heightfield from the cache, so try to create it.
                        // We also fallback on parent layers to make sure that we have data at the location even if it's fallback.
                        while (!layerHF.valid() && actualKey.valid() && layer->isKeyInLegalRange(actualKey))
                        {
                            layerHF = layer->createHeightfield(actualKey, ioc).value;
                            if (!layerHF.valid())
                            {
                                actualKey.makeParent();
                            }
                        }

                        // Mark this layer as fallback if necessary.
                        if (layerHF.valid())
                        {
                            //TODO: check this. Should it be actualKey != keyToUse...?
                            heightFallback[i] =
                                contenders[i].isFallback ||
                                (actualKey != contenderKey);

                            numHeightfieldsInCache++;
                        }
                        else
                        {
                            heightFailed[i] = true;
                            continue;
                        }
                    }

                    if (layerHF.valid())
                    {
                        bool isFallback = heightFallback[i];

                        // We only have real data if this is not a fallback heightfield.
                        if (!isFallback)
                        {
                            realData = true;
                        }

                        float elevation;
                        if (layerHF.getElevation(keySRS, x, y, interpolation, keySRS, elevation))
                        {
                            if (elevation != NO_DATA_VALUE)
                            {
                                // remember the index so we can only apply offset layers that
                                // sit on TOP of this layer.
                                resolvedIndex = index;

                                hf->write(fvec4(elevation), c, r);

                                resolution = actualKey.getResolution(hf->width()).second;
                            }
                            else
                            {
                                ++nodataCount;
                            }
                        }
                    }


                    // Clear the heightfield cache if we have too many heightfields in the cache.
                    if (numHeightfieldsInCache >= maxHeightfields)
                    {
                        //OE_NOTICE << "Clearing cache" << std::endl;
                        for (unsigned int k = 0; k < heightfields.size(); k++)
                        {
                            heightfields[k] = GeoHeightfield::INVALID;
                            heightFallback[k] = false;
                        }
                        numHeightfieldsInCache = 0;
                    }
                }

                for (int i = offsets.size() - 1; i >= 0; --i)
                {
                    if (ioc && ioc->isCanceled())
                        return false;

                    // Only apply an offset layer if it sits on top of the resolved layer
                    // (or if there was no resolved layer).
                    if (resolvedIndex >= 0 && offsets[i].index < resolvedIndex)
                        continue;

                    TileKey& contenderKey = offsets[i].key;

                    if (offsetFailed[i] == true)
                        continue;

                    GeoHeightfield& layerHF = offsetfields[i];
                    if (!layerHF.valid())
                    {
                        ElevationLayer* offset = offsets[i].layer.get();

                        layerHF = offset->createHeightfield(contenderKey, ioc).value;
                        if (!layerHF.valid())
                        {
                            offsetFailed[i] = true;
                            continue;
                        }
                    }

                    // If we actually got a layer then we have real data
                    realData = true;

                    float elevation = 0.0f;
                    if (layerHF.getElevation(keySRS, x, y, interpolation, keySRS, elevation) &&
                        elevation != NO_DATA_VALUE &&
                        !equivalent(elevation, 0.0f))
                    {
                        hf->heightAt(c, r) += elevation;

                        // Technically this is correct, but the resultin normal maps
                        // look awful and faceted.
                        resolution = std::min(
                            resolution,
                            (float)contenderKey.getResolution(hf->width()).second);
                    }
                }

                if (resolutions)
                {
                    (*resolutions)[r*numColumns+c] = resolution;
                }
            }
        }
    }

    // Resolve any invalid heights in the output heightfield.
    resolveInvalidHeights(hf.get(), key.getExtent(), NO_DATA_VALUE, nullptr);

    if (ioc && ioc->isCanceled())
    {
        return false;
    }

    // Return whether or not we actually read any real data
    return realData;
}
