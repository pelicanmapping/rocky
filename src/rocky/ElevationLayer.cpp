/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ElevationLayer.h"
#include "Geoid.h"
#include "Heightfield.h"
#include "json.h"

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


namespace
{
    class HeightfieldMosaic : public Inherit<Heightfield, HeightfieldMosaic>
    {
    public:
        HeightfieldMosaic(unsigned s, unsigned t) :
            super(s, t)
        {
            //nop
        }

        HeightfieldMosaic(const HeightfieldMosaic& rhs) :
            super(rhs),
            dependencies(rhs.dependencies)
        {
            //nop
        }

        virtual ~HeightfieldMosaic()
        {
            cleanupOperation();
        }

        std::vector<std::shared_ptr<Heightfield>> dependencies;
        std::function<void()> cleanupOperation;
    };
}

//------------------------------------------------------------------------

ElevationLayer::ElevationLayer() :
    super()
{
    construct({}, {});
}

ElevationLayer::ElevationLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON)
{
    construct(JSON, io);
}

void
ElevationLayer::construct(std::string_view JSON, const IOOptions& io)
{
    tileSize.set_default(257u); // override the default in TileLayer

    const auto j = parse_json(JSON);
    get_to(j, "offset", offset);
    get_to(j, "no_data_value", noDataValue);
    get_to(j, "min_valid_value", minValidValue);
    get_to(j, "max_valid_value", maxValidValue);
    std::string encoding_value;
    if (get_to(j, "encoding", encoding_value))
    {
        if (encoding_value == "single_channel")
            encoding = Encoding::SingleChannel;
        else if (encoding_value == "mapboxrgb")
            encoding = Encoding::MapboxRGB;
    }

    // a small L2 cache will help with things like normal map creation
    // (i.e. queries that sample neighboring tiles)
    if (!l2CacheSize.has_value())
    {
        l2CacheSize.set_default(32u);
    }

    _L2cache.setCapacity(l2CacheSize.value());

    // Disable max-level support for elevation data because it makes no sense.
    maxLevel.clear();
    maxResolution.clear();

    _dependencyCache = std::make_shared<TileMosaicWeakCache<Heightfield>>();
}

std::string
ElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "offset", offset);
    set(j, "no_data_value", noDataValue);
    set(j, "min_valid_value", minValidValue);
    set(j, "max_valid_value", maxValidValue);
    if (encoding.has_value(Encoding::SingleChannel))
        set(j, "encoding", "single_channel");
    else if (encoding.has_value(Encoding::MapboxRGB))
        set(j, "encoding", "mapboxrgb");

    return j.dump();
}

Result<>
ElevationLayer::openImplementation(const IOOptions& io)
{
    auto r = super::openImplementation(io);
    if (r.failed())
        return r;

    _dependencyCache = std::make_shared<TileMosaicWeakCache<Heightfield>>();

    return ResultVoidOK;
}

void
ElevationLayer::closeImplementation()
{
    _dependencyCache = nullptr;
    super::closeImplementation();
}

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
                equiv(h, noDataValue.value()) ||
                h < minValidValue ||
                h > maxValidValue)
            {
                *pixel = NO_DATA_VALUE;
            }
        }
    }
}

std::pair<Distance, Distance>
ElevationLayer::resolution(unsigned level) const
{
    if (!profile.valid())
        return {};

    auto dims = profile.tileDimensions(level);

    return std::make_pair(
        Distance(dims.x / (double)(tileSize.value() - 1), profile.srs().units()),
        Distance(dims.y / (double)(tileSize.value() - 1), profile.srs().units()));
}

std::shared_ptr<Heightfield>
ElevationLayer::assembleHeightfield(const TileKey& key, const IOOptions& io) const
{
    std::shared_ptr<HeightfieldMosaic> output;

    // Determine the intersecting keys
    unsigned targetLOD = profile.equivalentLOD(key.profile, key.level);
    auto intersectingKeys = key.intersectingKeys(profile);

    // collect heightfield for each intersecting key. Note, we're hitting the
    // underlying tile source here, so there's no vetical datum shifts happening yet.
    // we will do that later.
    std::vector<GeoHeightfield> sources;

    if (intersectingKeys.size() > 0)
    {
        bool hasAtLeastOneSourceAtTargetLOD = false;

        for (auto& intersectingKey : intersectingKeys)
        {
            // first try the weak dependency cache.
            auto cached = _dependencyCache->get(intersectingKey);
            auto cached_value = cached.value.lock();
            if (cached_value)
            {
                sources.emplace_back(cached_value, cached.valueKey.extent());

                if (cached.valueKey.level == targetLOD)
                {
                    hasAtLeastOneSourceAtTargetLOD = true;
                }
            }

            else
            {
                TileKey subKey = intersectingKey;
                GeoHeightfield subTile;
                while (subKey.valid() && (!subTile.valid() || !subTile.heightfield()))
                {
                    auto r = createHeightfieldImplementation_internal(subKey, io);
                    if (r.ok())
                        subTile = r.release();
                    else
                        subKey.makeParent();

                    if (io.canceled())
                        return nullptr;
                }

                if (subTile.valid() && subTile.heightfield())
                {
                    // save it in the weak cache:
                    _dependencyCache->put(intersectingKey, subKey, subTile.heightfield());

                    // add it to our sources collection:
                    sources.emplace_back(std::move(subTile));

                    if (subKey.level == targetLOD)
                    {
                        hasAtLeastOneSourceAtTargetLOD = true;
                    }
                }
            }
        }

        // If we actually got at least one piece of usable data,
        // move ahead and build a mosaic of all sources.
        if (hasAtLeastOneSourceAtTargetLOD)
        {
            unsigned cols = 0;
            unsigned rows = 0;

            // output size is the max of all the source sizes.
            for (auto& source : sources)
            {
                cols = std::max(cols, source.heightfield()->width());
                rows = std::max(rows, source.heightfield()->height());
            }

            // assume all tiles to mosaic are in the same SRS.
            SRSOperation xform = key.extent().srs().to(sources[0].srs());

            // Now sort the heightfields by resolution to make sure we're sampling
            // the highest resolution one first.
            std::sort(sources.begin(), sources.end(), GeoHeightfield::SortByResolutionFunctor());

            // new output HF:
            output = HeightfieldMosaic::create(cols, rows);
            output->fill(NO_DATA_VALUE);

            // Cache pointers to the source images that mosaic to create this tile.
            output->dependencies.reserve(sources.size());
            for (auto& source : sources)
                output->dependencies.push_back(source.heightfield());

            // Clean up orphaned entries any time a tile destructs.
            output->cleanupOperation = [captured{ std::weak_ptr(_dependencyCache) }, key]() {
                auto cache = captured.lock();
                if (cache)
                    cache->clean();
                };

            // working set of points. it's much faster to xform an entire vector all at once.
            std::vector<glm::dvec3> points;
            points.resize(cols * rows);

            // note, for elevation we sample edge to edge instead of on pixel-center.
            double minx, miny, maxx, maxy;
            key.extent().getBounds(minx, miny, maxx, maxy);
            double dx = (maxx - minx) / (double)(cols-1);
            double dy = (maxy - miny) / (double)(rows-1);

            // build a grid of sample points:
            for (unsigned r = 0; r < rows; ++r)
            {
                double y = miny + (dy * (double)r);
                for (unsigned c = 0; c < cols; ++c)
                {
                    double x = minx + (dx * (double)c);
                    points[r * cols + c] = { x, y, 0.0 };
                }
            }

            // transform the sample points to the SRS of our source data tiles.
            // Note, point.z will hold a vdatum offset if applicable.
            if (xform.valid())
            {
                xform.transformArray(&points[0], points.size());
            }

            // sample the heights:
            for (unsigned r = 0; r < rows; ++r)
            {
                for (unsigned c = 0; c < cols; ++c)
                {
                    auto& point = points[r * cols + c];
                    float& height = output->heightAt(c, r);
                    for (unsigned i = 0; height == NO_DATA_VALUE && i < sources.size(); ++i)
                    {
                        height = sources[i].heightAtLocation(point.x, point.y, Interpolation::Bilinear);
                    }

                    if (height != NO_DATA_VALUE)
                    {
                        height -= point.z; // apply reverse vdatum offset
                    }
                }
            }
        }
    }

    // If the progress was cancelled clear out any of the output data.
    if (io.canceled())
    {
        output = nullptr;
    }

    return output;
}

Result<GeoHeightfield>
ElevationLayer::createHeightfield(const TileKey& key, const IOOptions& io) const
{
    std::shared_lock readLock(layerStateMutex());
    if (status().ok())
    {
        if (io.services().residentImageCache)
        {
            auto k = key.str() + '-' + std::to_string(uid()) + "-" + std::to_string(revision());

            auto image = io.services().residentImageCache->get(k);
            if (image.has_value())
            {
                auto hf = std::dynamic_pointer_cast<Heightfield>(image.value());
                return GeoHeightfield(hf, key.extent());
            }

            auto r = createHeightfieldInKeyProfile(key, io);

            if (r.ok())
            {
                io.services().residentImageCache->put(k, r.value().heightfield());
            }

            return r;
        }
        else
        {
            return createHeightfieldInKeyProfile(key, io);
        }
    }
    else
    {
        return status().error();
    }
}

Result<GeoHeightfield>
ElevationLayer::createHeightfieldImplementation_internal(const TileKey& key, const IOOptions& io) const
{
    std::shared_lock lock(layerStateMutex());
    auto result = createHeightfieldImplementation(key, io);
    if (result.failed())
    {
        Log()->debug("Failed to create heightfield for key {0} : {1}", key.str(), result.error().message);
    }
    return result;
}

Result<GeoHeightfield>
ElevationLayer::createHeightfieldInKeyProfile(const TileKey& key, const IOOptions& io) const
{
    GeoHeightfield result;
    std::shared_ptr<Heightfield> hf;

    auto my_profile = profile;

    if (!my_profile.valid() || !isOpen())
    {
        return Failure(Failure::ResourceUnavailable, "Layer not open or initialize");
    }

    // Check that the key is legal (in valid LOD range, etc.)
    if ( !isKeyInLegalRange(key) )
    {
        return Failure_ResourceUnavailable;
    }

    if (key.profile.equivalentTo(my_profile))
    {
        auto r = createHeightfieldImplementation_internal(key, io);

        if (r.failed())
            return r;
        else
            result = r.value();
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto hf = assembleHeightfield(key, io);
        result = GeoHeightfield(hf, key.extent());
    }

    // Check for cancelation before writing to a cache
    if (io.canceled())
    {
        return Failure_ResourceUnavailable;
    }

    // The const_cast is safe here because we just created the
    // heightfield from scratch...not from a cache.
    hf = result.heightfield();

    // validate it to make sure it's legal.
    if (hf && !validateHeightfield(hf.get()))
    {
        return Failure(Failure::GeneralError, "Generated an illegal heightfield!");
    }

    // Pre-caching operations:
    normalizeNoDataValues(hf.get());

    // No luck on any path:
    if (hf == nullptr)
    {
        return Failure(Failure::ResourceUnavailable);
    }

    result = GeoHeightfield(hf, key.extent());

    return result;
}

#undef  LC
#define LC "[ElevationLayers] "

namespace
{
    struct LayerData
    {
        std::shared_ptr<ElevationLayer> layer;
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
                ex.srs().isGeodetic() ? ex :
                ex.transform(ex.srs().geodeticSRS());
            double latMin = geodeticExtent.ymin();
            double lonMin = geodeticExtent.xmin();
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
    std::shared_ptr<Heightfield> hf,
    std::vector<float>* resolutions,
    const TileKey& key,
    const Profile& haeProfile,
    Interpolation interpolation,
    const IOOptions& io) const
{
    // heightfield must already exist.
    if ( !hf )
        return false;

    // if the caller provided an "HAE map profile", he wants an HAE elevation grid even if
    // the map profile has a vertical datum. This is the usual case when building the 3D
    // terrain, for example. Construct a temporary key that doesn't have the vertical
    // datum info and use that to query the elevation data.
    TileKey keyToUse = key;
    if (haeProfile.valid())
    {
        keyToUse = TileKey(key.level, key.x, key.y, haeProfile);
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
    for (i = (int)size()-1; i>=0; --i)
    {
        auto layer = (*this)[i];

        if (layer->isOpen())
        {
            // calculate the resolution-mapped key (adjusted for tile resolution differential).
            TileKey mappedKey = keyToUse.mapResolution(
                hf->width(),
                layer->tileSize );

            bool useLayer = true;
            TileKey bestKey( mappedKey );

            // Check whether the non-mapped key is valid according to the user's minLevel setting.
            // We wll ignore the maxDataLevel setting, because we account for that by getting
            // the "best available" key later. We must keep these layers around in case we need
            // to fill in empty spots.
            if (key.level < layer->minLevel)
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
                bestKey = layer->bestAvailableTileKey(mappedKey);
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
                if ( layer->offset == true)
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
    double   xmin = key.extent().xmin();
    double   ymin = key.extent().ymin();
    double   dx = key.extent().width() / (double)(numColumns - 1);
    double   dy = key.extent().height() / (double)(numRows - 1);

    auto keySRS = keyToUse.profile.srs();

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
        if (layerHF.ok() && layerHF.value().valid())
        {
            if (layerHF->heightfield()->width() == hf->width() &&
                layerHF->heightfield()->height() == hf->height())
            {
                requiresResample = false;

                memcpy(
                    hf->data<unsigned char>(),
                    layerHF->heightfield()->data<unsigned char>(),
                    hf->sizeInBytes());

                realData = true;

                if (resolutions)
                {
                    auto [resx, resy] = contenders[0].key.getResolutionForTileSize(hf->width());
                    for (unsigned i = 0; i < hf->width()*hf->height(); ++i)
                        (*resolutions)[i] = (float)resy;
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

                glm::fvec3 normal_sum(0, 0, 0);

                for (int i = 0; i < contenders.size() && resolvedIndex < 0; ++i)
                {
                    ElevationLayer* layer = contenders[i].layer.get();
                    TileKey& contenderKey = contenders[i].key;
                    int index = contenders[i].index;

                    if (heightFailed[i])
                        continue;

                    GeoHeightfield layerHF = heightfields[i];
                    TileKey& actualKey = heightfieldActualKeys[i];

                    if (!layerHF.valid())
                    {
                        // We couldn't get the heightfield from the cache, so try to create it.
                        // We also fallback on parent layers to make sure that we have data at the location even if it's fallback.
                        while (!layerHF.valid() && actualKey.valid() && layer->isKeyInLegalRange(actualKey))
                        {
                            auto temp = layer->createHeightfield(actualKey, io);
                            if (temp.ok() && temp.value().valid())
                                layerHF = temp.value();
                            else
                                actualKey.makeParent();
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

                        //TODO: optimize by using SRSOperation variant
                        float elevation = layerHF.heightAt(x, y, keySRS, interpolation);
                        if (elevation != NO_DATA_VALUE)
                        {
                            // remember the index so we can only apply offset layers that
                            // sit on TOP of this layer.
                            resolvedIndex = index;

                            hf->write(glm::fvec4(elevation), c, r);

                            resolution = (float)actualKey.getResolutionForTileSize(hf->width()).second;
                        }
                        else
                        {
                            ++nodataCount;
                        }
                    }


                    // Clear the heightfield cache if we have too many heightfields in the cache.
                    if (numHeightfieldsInCache >= maxHeightfields)
                    {
                        //OE_NOTICE << "Clearing cache" << std::endl;
                        for (unsigned int k = 0; k < heightfields.size(); k++)
                        {
                            heightfields[k] = {};
                            heightFallback[k] = false;
                        }
                        numHeightfieldsInCache = 0;
                    }
                }

                for (int i = (int)offsets.size() - 1; i >= 0; --i)
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

                    GeoHeightfield layerHF = offsetfields[i];
                    if (!layerHF.valid())
                    {
                        ElevationLayer* offset = offsets[i].layer.get();

                        auto r = offset->createHeightfield(contenderKey, io);
                        if (r.ok() && r.value().valid()) {
                            layerHF = r.value();
                        }
                        else {
                            offsetFailed[i] = true;
                            continue;
                        }
                    }

                    // If we actually got a layer then we have real data
                    realData = true;

                    float elevation = layerHF.heightAt(x, y, keySRS, interpolation);
                    if (elevation != NO_DATA_VALUE && !equiv(elevation, 0.0f))
                    {
                        hf->heightAt(c, r) += elevation;

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

std::shared_ptr<Heightfield>
ElevationLayer::decodeRGB(std::shared_ptr<Image> image) const
{
    if (!image || !image->valid())
        return nullptr;

    // For RGB-encoded elevation, we want read the components in their raw form
    // and not perform any color space conversion.
    auto view_format =
        image->pixelFormat() == Image::R8_SRGB ? Image::R8_UNORM :
        image->pixelFormat() == Image::R8G8_SRGB ? Image::R8G8_UNORM :
        image->pixelFormat() == Image::R8G8B8_SRGB ? Image::R8G8B8_UNORM :
        image->pixelFormat() == Image::R8G8B8A8_SRGB ? Image::R8G8B8A8_UNORM :
        image->pixelFormat();

    Image view = image->viewAs(view_format);

    // convert the RGB Elevation into an actual heightfield
    auto hf = Heightfield::create(view.width(), view.height());

    glm::fvec4 pixel;

    if (encoding == Encoding::TerrariumRGB)
    {
        view.eachPixel([&](auto& i)
            {
                view.read(pixel, i.s(), i.t());

                float height = ((pixel.r * 255.0f * 256.0f + pixel.g * 255.0f + pixel.b * 255.0f / 256.0f) - 32768.0f);

                if (height < -9999 || height > 999999)
                    height = NO_DATA_VALUE;

                hf->heightAt(i.s(), i.t()) = height;
            });
    }

    else // default to MapboxRGB
    {
        view.eachPixel([&](auto& i)
            {
                view.read(pixel, i.s(), i.t());

                float height = -10000.f + ((pixel.r * 256.0f * 256.0f + pixel.g * 256.0f + pixel.b) * 256.0f * 0.1f);

                if (height < -9999 || height > 999999)
                    height = NO_DATA_VALUE;

                hf->heightAt(i.s(), i.t()) = height;
            });
    }

    return hf;
}
