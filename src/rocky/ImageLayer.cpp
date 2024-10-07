/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "ImageLayer.h"

#include "Color.h"
#include "IOTypes.h"
#include "Metrics.h"
#include "Utils.h"
#include "GeoImage.h"
#include "Image.h"
#include "TileKey.h"
#include "json.h"

#include <cinttypes>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[ImageLayer] \"" << name().value() << "\" "

namespace
{
    class CompositeImage : public Inherit<Image, CompositeImage>
    {
    public:
        CompositeImage(
            PixelFormat format,
            unsigned s,
            unsigned t,
            unsigned r = 1) :
            super(format, s, t, r),
            dependencies(),
            cleanupOperation()
        {}

        CompositeImage(const CompositeImage& rhs) :
            super(rhs),
            dependencies(rhs.dependencies),
            cleanupOperation(rhs.cleanupOperation)
        {}

        virtual ~CompositeImage()
        {
            if (cleanupOperation)
                cleanupOperation();
        }

        std::shared_ptr<Image> clone() const override
        {
            return std::make_shared<CompositeImage>(*this);
        }

        std::vector<std::shared_ptr<Image>> dependencies;
        std::function<void()> cleanupOperation;
    };
}

ImageLayer::ImageLayer() :
    super()
{
    construct({}, {});
}

ImageLayer::ImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON)
{
    construct(JSON, io);
}

void
ImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    const auto j = parse_json(JSON);
    get_to(j, "sharpness", _sharpness);
    get_to(j, "crop", _crop);

    setRenderType(RenderType::TERRAIN_SURFACE);

    _dependencyCache = std::make_shared<TileMosaicWeakCache<Image>>();
}

JSON
ImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "sharpness", _sharpness);
    set(j, "crop", _crop);
    return j.dump();
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key, const IOOptions& io) const
{    
    if (!isOpen())
    {
        return Result(GeoImage::INVALID);
    }

    auto result = createImageInKeyProfile(key, io);

    return result;
}

Result<GeoImage>
ImageLayer::createImageImplementation_internal(const TileKey& key, const IOOptions& io) const
{
    std::shared_lock lock(layerStateMutex());
    auto result = createImageImplementation(key, io);
    if (result.status.failed())
    {
        Log()->debug("Failed to create image for key {0} : {1}", key.str(), result.status.message);
    }
    return result;
}

Result<GeoImage>
ImageLayer::createImageInKeyProfile(const TileKey& key, const IOOptions& io) const
{
    // Make sure the request is in range.
    // TODO: perhaps this should be a call to mayHaveData(key) instead.
    if ( !isKeyInLegalRange(key) )
    {
        return GeoImage::INVALID;
    }

    Result<GeoImage> result;
    float sharpness = _sharpness.has_value() ? _sharpness.value() : 0.0f;

    GeoExtent cropIntersection;

    // if we are cropping, and the key doesn't intersect the crop extent, bail out.
    if (_crop.has_value() && _crop.value().valid())
    {
        auto cropInKeyProfile = key.profile().clampAndTransformExtent(_crop.value());
        if (cropInKeyProfile.valid())
        {
            cropIntersection = cropInKeyProfile.intersectionSameSRS(key.extent());
            if (!cropIntersection.valid())
            {
                return GeoImage::INVALID;
            }
        }
    }

    // if this layer has no profile, just go straight to the driver.
    if (!profile().valid())
    {
        result = createImageImplementation_internal(key, io);
    }

    else if (key.profile().horizontallyEquivalentTo(profile()))
    {
        result = createImageImplementation_internal(key, io);
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto image = assembleImage(key, io);
        result = GeoImage(image, key.extent());

        // automatically re-sharpen a reprojected image to account for quality loss.
        // Do we like this idea?
        if (sharpness == 0.0f)
            sharpness = 2.0f;
    }

    // developer assert - if a status is OK, the image must exist.
    // address this later if necessary.
    //ROCKY_SOFT_ASSERT(result.status.failed() || result.value.image());

    if (result.status.ok() && result.value.image())
    {
        if (cropIntersection.valid())
        {
            auto b = key.extent().bounds();
            if (!cropIntersection.contains(b))
            {
                int s0, t0, s1, t1;
                result.value.getPixel(cropIntersection.xmin(), cropIntersection.ymin(), s0, t0);
                result.value.getPixel(cropIntersection.xmax(), cropIntersection.ymax(), s1, t1);
                s0 = clamp(s0, 0, (int)result.value.image()->width() - 1);
                s1 = clamp(s1, 0, (int)result.value.image()->width() - 1);
                t0 = clamp(t0, 0, (int)result.value.image()->height() - 1);
                t1 = clamp(t1, 0, (int)result.value.image()->height() - 1);

                auto image = result.value.image();
                image->get_iterator().forEachPixel([&](auto& i)
                    {
                        if ((int)i.s() < s0 || (int)i.s() > s1 || (int)i.t() < t0 || (int)i.t() > t1)
                        {
                            image->write({ 0,0,0,0 }, i.s(), i.t());
                        }
                    }
                );
            }
        }

        if (sharpness > 0.0f)
        {
            result = GeoImage(result.value.image()->sharpen(sharpness), key.extent());
        }
    }

    return result;
}

shared_ptr<Image>
ImageLayer::assembleImage(const TileKey& key, const IOOptions& io) const
{
    std::shared_ptr<CompositeImage> output;

    // Map the key's LOD to the target profile's LOD.
    unsigned targetLOD = profile().getEquivalentLOD(key.profile(), key.levelOfDetail());

    // Find the set of keys that covers the same area as in input key in our target profile.
    std::vector<TileKey> intersectingKeys;
    key.getIntersectingKeys(profile(), intersectingKeys);

#if 0 // Re-evaluate this later if needed (gw)
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
            std::vector<TileKey> temp;
            TileKey::getIntersectingKeys(
                key.extent(),
                intersectionLOD,
                profile(),
                temp);

            for (auto& layerKey : intersectingKeys)
            {
                if (mayHaveData(layerKey))
                {
                    ++numTilesThatMayHaveData;
                }
            }

            --intersectionLOD;
        }

        targetLOD = intersectionLOD;
    }
#endif

    // collect raster data for each intersecting key, falling back on ancestor images
    // if none are available at the target LOD.
    std::vector<GeoImage> sources;

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

                if (cached.valueKey.levelOfDetail() == targetLOD)
                {
                    hasAtLeastOneSourceAtTargetLOD = true;
                }
            }

            else
            {
                // create the sub tile, falling back until we get real data
                TileKey subKey = intersectingKey;
                Result<GeoImage> subTile;
                while (subKey.valid() && !subTile.status.ok())
                {
                    subTile = createImageImplementation_internal(subKey, io);

                    if (subTile.status.failed() || subTile.value.image() == nullptr)
                        subKey.makeParent();

                    if (io.canceled())
                        return {};
                }

                if (subTile.status.ok() && subTile.value.image())
                {
                    // save it in the weak cache:
                    _dependencyCache->put(intersectingKey, subKey, subTile.value.image());

                    // add it to our sources collection:
                    sources.emplace_back(subTile.value);

                    if (subKey.levelOfDetail() == targetLOD)
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

            // sort the sources by resolution (highest first)
            std::sort(
                sources.begin(), sources.end(),
                [](const GeoImage& lhs, const GeoImage& rhs) {
                    return lhs.extent().width() < rhs.extent().width();
                });

            // output size is the max of all the source sizes.
            for(auto& source : sources)
            {
                cols = std::max(cols, source.image()->width());
                rows = std::max(rows, source.image()->height());
            }

            // assume all tiles to mosaic are in the same SRS.
            SRSOperation xform = key.extent().srs().to(sources[0].srs());
            
            // Working bounds of the SRS itself so we can clamp out-of-bounds points.
            // This is especially important when going from Mercator to Geographic
            // where there's no data beyond +/- 85 degrees.
            auto sourceBounds = sources[0].srs().bounds();

            // new output:
            output = CompositeImage::create(Image::R8G8B8A8_UNORM, cols, rows);

            // Cache pointers to the source images that mosaic to create this tile.
            output->dependencies.reserve(sources.size());
            for (auto& source : sources)
                output->dependencies.push_back(source.image());

            // Clean up orphaned entries any time a tile destructs.
            output->cleanupOperation = [captured{ std::weak_ptr(_dependencyCache) }, key]() {
                auto cache = captured.lock();
                if (cache)
                    cache->clean();
                };

            // Working set of points. it's much faster to xform an entire vector all at once.
            std::vector<glm::dvec3> points;
            points.assign(cols*rows, { 0, 0, 0 });

            double minx, miny, maxx, maxy;
            key.extent().getBounds(minx, miny, maxx, maxy);
            double dx = (maxx - minx) / (double)(cols);
            double dy = (maxy - miny) / (double)(rows);

            // build a grid of sample points:
            for (unsigned r = 0; r < rows; ++r)
            {
                double y = miny + (0.5*dy) + (dy * (double)r);
                for (unsigned c = 0; c < cols; ++c)
                {
                    double x = minx + (0.5*dx) + (dx * (double)c);
                    points[r * cols + c] = { x, y, 0.0 };
                }
            }

            // shrink the sourceBounds by our pixel-centering value:
            if (sourceBounds.valid())
            {
                sourceBounds.xmin += 0.5 * dx;
                sourceBounds.ymin += 0.5 * dy;
                sourceBounds.xmax -= 0.5 * dx;
                sourceBounds.ymax -= 0.5 * dy;
            }

            // transform the sample points to the SRS of our source data tiles:
            if (xform.valid())
            {
                xform.transformArray(&points[0], points.size());

                // clamp the transformed points to the profile SRS.
                if (sourceBounds.valid())
                {
                    for (auto& point : points)
                    {
                        sourceBounds.clamp(point.x, point.y, point.z);
                    }
                }
            }

            // Mosaic our sources into a single output image.
            glm::fvec4 pixel;
            for (unsigned r = 0; r < rows; ++r)
            {
                for (unsigned c = 0; c < cols; ++c)
                {
                    unsigned i = r * cols + c;

                    // check each source (high to low LOD) until we get a valid pixel.
                    pixel = { 0,0,0,0 };

                    for (unsigned k = 0; k < sources.size(); ++k)
                    {
                        if (sources[k].read(pixel, points[i].x, points[i].y) && pixel.a > 0.0f)
                        {
                            break;
                        }
                    }

                    output->write(pixel, c, r);
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
