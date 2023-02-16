/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ElevationLayer.h"
#include "Geoid.h"
#include "Heightfield.h"
#include "Metrics.h"

#include <cinttypes>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[ElevationLayer] \"" << name().value() << "\" : "

namespace
{
    // perform very basic sanity-check validation on a heightfield.
    bool validateHeightfield(const Heightfield* hf)
    {
        if (!hf)
            return false;
        if (hf->height() < 1 || hf->height() > 1024) {
            //ROCKY_WARN << "row count = " << hf->height() << std::endl;
            return false;
        }
        if (hf->width() < 1 || hf->width() > 1024) {
            //ROCKY_WARN << "col count = " << hf->width() << std::endl;
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
    _tileSize.set_default(257u); // override the default in TileLayer

    conf.get("offset", _offset);
    conf.get("nodata_value", _noDataValue);
    conf.get("no_data_value", _noDataValue);
    conf.get("min_valid_Value", _minValidValue);
    conf.get("max_valid_value", _maxValidValue);

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

    // a small L2 cache will help with things like normal map creation
    // (i.e. queries that sample neighboring tiles)
    if (!_l2cachesize.has_value())
    {
        _l2cachesize = 4u;
    }

    // Disable max-level support for elevation data because it makes no sense.
    _maxLevel.clear();
    _maxResolution.clear();

    // elevation layers do not render directly; rather, a composite of elevation data
    // feeds the terrain engine to permute the mesh.
    //setRenderType(RENDERTYPE_NONE);
}

Config
ElevationLayer::getConfig() const
{
    Config conf = TileLayer::getConfig();
    conf.set("offset", _offset);
    conf.set("no_data_value", _noDataValue);
    conf.set("min_valid_Value", _minValidValue);
    conf.set("max_valid_value", _maxValidValue);
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
                equiv(h, noDataValue().value()) ||
                h < minValidValue() ||
                h > maxValidValue())
            {
                *pixel = NO_DATA_VALUE;
            }
        }
    }
}

void
ElevationLayer::assembleHeightfield(
    const TileKey& key,
    shared_ptr<Heightfield> out_hf,
    const IOOptions& io) const
{
    // Collect the heightfields for each of the intersecting tiles.
    std::vector<GeoHeightfield> geohf_list;

    // Determine the intersecting keys
    std::vector<TileKey> intersectingKeys;

    if (key.levelOfDetail() > 0u)
    {
        key.getIntersectingKeys(profile(), intersectingKeys);
    }

    else
    {
        // LOD is zero - check whether the LOD mapping went out of range, and if so,
        // fall back until we get valid tiles. This can happen when you have two
        // profiles with very different tile schemes, and the "equivalent LOD"
        // surpasses the max data LOD of the tile source.
        unsigned numTilesThatMayHaveData = 0u;

        int intersectionLOD = profile().getEquivalentLOD(key.profile(), key.levelOfDetail());

        while (numTilesThatMayHaveData == 0u && intersectionLOD >= 0)
        {
            intersectingKeys.clear();

            TileKey::getIntersectingKeys(
                key.extent(),
                intersectionLOD,
                profile(),
                intersectingKeys);

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
                std::shared_lock L(layerMutex());

                auto result = createHeightfieldImplementation(layerKey, io);

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
                width = std::max(width, geohf.heightfield()->width());
                height = std::max(height, geohf.heightfield()->height());
            }

            // Now sort the heightfields by resolution to make sure we're sampling
            // the highest resolution one first.
            std::sort(geohf_list.begin(), geohf_list.end(), GeoHeightfield::SortByResolutionFunctor());

            // new output HF:
            out_hf = Heightfield::create(width, height);

            //Go ahead and set up the heightfield so we don't have to worry about it later
            double minx, miny, maxx, maxy;
            key.extent().getBounds(minx, miny, maxx, maxy);
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
                        dvec3 point(x, y, 0);

                        if (geohf.getElevation(key.extent().srs(), point, Heightfield::BILINEAR))
                        {
                            elevation = point.z;
                            break;
                        }
                    }

                    out_hf->write(fvec4(elevation), c, r);
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
    if (io.canceled())
    {
        out_hf = nullptr;
    }
}

Result<GeoHeightfield>
ElevationLayer::createHeightfield(const TileKey& key) const
{
    return createHeightfield(key, IOOptions());
}

Result<GeoHeightfield>
ElevationLayer::createHeightfield(
    const TileKey& key,
    const IOOptions& io) const
{    
    // If the layer is disabled, bail out
    if (!isOpen())
    {
        return Result(GeoHeightfield::INVALID);
    }

    return createHeightfieldInKeyProfile(key, io);
}

Result<GeoHeightfield>
ElevationLayer::createHeightfieldInKeyProfile(
    const TileKey& key,
    const IOOptions& io) const
{
    GeoHeightfield result;
    shared_ptr<Heightfield> hf;

    auto my_profile = profile();
    if (!my_profile.valid() || !isOpen())
    {
        return Result<GeoHeightfield>(Status::ResourceUnavailable, "Layer not open or initialize");
    }

    // Check that the key is legal (in valid LOD range, etc.)
    if ( !isKeyInLegalRange(key) )
    {
        return Result(GeoHeightfield::INVALID);
    }

    if (key.profile() == my_profile)
    {
        std::shared_lock L(layerMutex());
        auto r = createHeightfieldImplementation(key, io);
        if (r.status.failed())
            return r;
        else
            result = r.value;
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        shared_ptr<Heightfield> hf;
        assembleHeightfield(key, hf, io);
        result = GeoHeightfield(hf, key.extent());
    }

    // Check for cancelation before writing to a cache
    if (io.canceled())
    {
        return Result(GeoHeightfield::INVALID);
    }

    // The const_cast is safe here because we just created the
    // heightfield from scratch...not from a cache.
    hf = result.heightfield();

    // validate it to make sure it's legal.
    if (hf && !validateHeightfield(hf.get()))
    {
        return Result<GeoHeightfield>(Status::GeneralError, "Generated an illegal heightfield!");
    }

    // Pre-caching operations:
    normalizeNoDataValues(hf.get());

    // No luck on any path:
    if (hf == nullptr)
    {
        return Result(GeoHeightfield::INVALID);
    }

    return GeoHeightfield(hf, key.extent());
}

Status
ElevationLayer::writeHeightfield(
    const TileKey& key,
    const Heightfield* hf,
    const IOOptions& io) const
{
    if (isWritingSupported() && isWritingRequested())
    {
        std::shared_lock L(layerMutex());
        return writeHeightfieldImplementation(key, hf, io);
    }
    return Status(Status::ServiceUnavailable);
}

Status
ElevationLayer::writeHeightfieldImplementation(
    const TileKey& key,
    const Heightfield* hf,
    const IOOptions& io) const
{
    return Status(Status::ServiceUnavailable);
}

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
                ex.srs().isGeographic() ? ex :
                ex.transform(ex.srs().geoSRS());
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
    const Profile& haeProfile,
    Heightfield::Interpolation interpolation,
    const IOOptions& io) const
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
    if (haeProfile.valid())
    {
        keyToUse = TileKey(key.levelOfDetail(), key.tileX(), key.tileY(), haeProfile);
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
            if (key.levelOfDetail() < layer->minLevel())
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
    double   xmin = key.extent().xMin();
    double   ymin = key.extent().yMin();
    double   dx = key.extent().width() / (double)(numColumns - 1);
    double   dy = key.extent().height() / (double)(numRows - 1);

    auto keySRS = keyToUse.profile().srs();

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

        auto layerHF = layer->createHeightfield(contenders[0].key, io);
        if (layerHF.value.valid())
        {
            if (layerHF->heightfield()->width() == hf->width() &&
                layerHF->heightfield()->height() == hf->height())
            {
                requiresResample = false;

                memcpy(
                    hf->data<unsigned char>(),
                    layerHF->heightfield()->data<unsigned char>(),
                    hf->sizeInBytes());
                //memcpy(hf->getFloatArray()->asVector().data(),
                //    layerHF.getHeightfield()->getFloatArray()->asVector().data(),
                //    sizeof(float) * hf->getFloatArray()->size()
                //);

                realData = true;

                if (resolutions)
                {
                    auto [resx, resy] = contenders[0].key.getResolutionForTileSize(hf->width());
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
            if (io.canceled())
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
                            layerHF = layer->createHeightfield(actualKey, io).value;
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

                        dvec3 temp(x, y, 0);
                        if (layerHF.getElevation(keySRS, temp, interpolation))
                        {
                            if (temp.z != NO_DATA_VALUE)
                            {
                                // remember the index so we can only apply offset layers that
                                // sit on TOP of this layer.
                                resolvedIndex = index;

                                hf->write(fvec4(temp.z), c, r);

                                resolution = actualKey.getResolutionForTileSize(hf->width()).second;
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
                    if (io.canceled())
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

                        layerHF = offset->createHeightfield(contenderKey, io).value;
                        if (!layerHF.valid())
                        {
                            offsetFailed[i] = true;
                            continue;
                        }
                    }

                    // If we actually got a layer then we have real data
                    realData = true;

                    dvec3 temp(x, y, 0);
                    if (layerHF.getElevation(keySRS, temp, interpolation) &&
                        temp.z != NO_DATA_VALUE &&
                        !equiv(temp.z, 0.0))
                    {
                        hf->heightAt(c, r) += temp.z;

                        // Technically this is correct, but the resultin normal maps
                        // look awful and faceted.
                        resolution = std::min(
                            resolution,
                            (float)contenderKey.getResolutionForTileSize(hf->width()).second);
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
    resolveInvalidHeights(hf.get(), key.extent(), NO_DATA_VALUE, nullptr);

    if (io.canceled())
    {
        return false;
    }

    // Return whether or not we actually read any real data
    return realData;
}
