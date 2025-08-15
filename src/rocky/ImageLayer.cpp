/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "ImageLayer.h"

#include "Color.h"
#include "IOTypes.h"
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
    // Image subclass that keeps a list of the images used to assemble it.
    // This in combination with the ImageLayer's dependency tracker acts
    // as a resident dependency cache, speeding up assemble operations.
    class Mosaic : public Inherit<Image, Mosaic>
    {
    public:
        Mosaic(PixelFormat format, unsigned s, unsigned t, unsigned r = 1) :
            super(format, s, t, r),
            dependencies(),
            cleanupOperation()
        {}

        Mosaic(const Mosaic& rhs) :
            super(rhs),
            dependencies(rhs.dependencies),
            cleanupOperation(rhs.cleanupOperation)
        {}

        virtual ~Mosaic()
        {
            if (cleanupOperation)
                cleanupOperation();
        }

        std::shared_ptr<Image> clone() const override
        {
            return std::make_shared<Mosaic>(*this);
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

ImageLayer::ImageLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON)
{
    construct(JSON, io);
}

void
ImageLayer::construct(std::string_view JSON, const IOOptions& io)
{
    const auto j = parse_json(JSON);
    get_to(j, "sharpness", sharpness);
    get_to(j, "crop", crop);
}

std::string
ImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "sharpness", sharpness);
    set(j, "crop", crop);
    return j.dump();
}

Result<>
ImageLayer::openImplementation(const IOOptions& io)
{
    auto r = super::openImplementation(io);
    if (r.failed())
        return r;

    _dependencyCache = std::make_shared<TileMosaicWeakCache<Image>>();

    return ResultVoidOK;
}

void
ImageLayer::closeImplementation()
{
    _dependencyCache = nullptr;
    super::closeImplementation();
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key, const IOOptions& io) const
{
    std::shared_lock readLock(layerStateMutex());
    if (status().ok())
    {
        if (io.services().residentImageCache)
        {
            auto k = key.str() + '-' + std::to_string(key.profile.hash()) + '-' + std::to_string(uid()) + "-" + std::to_string(revision());

            auto image = io.services().residentImageCache->get(k);
            if (image.has_value())
                return GeoImage(image.value(), key.extent());

            auto r = createImageInKeyProfile(key, io);

            if (r.ok())
            {
                io.services().residentImageCache->put(k, r.value().image());
            }

            return r;
        }
        else
        {
            return createImageInKeyProfile(key, io);
        }
    }
    else
    {
        return status().error();
    }
}

Result<GeoImage>
ImageLayer::createImageImplementation_internal(const TileKey& key, const IOOptions& io) const
{
    std::shared_lock lock(layerStateMutex());
    auto result = createImageImplementation(key, io);
    if (result.failed())
    {
        Log()->debug("Failed to create image for key {0} : {1}", key.str(), result.error().message);
    }
    return result;
}

Result<GeoImage>
ImageLayer::createImageInKeyProfile(const TileKey& key, const IOOptions& io) const
{
    Result<GeoImage> result = Failure_ResourceUnavailable;

    if (!intersects(key))
        return Failure_ResourceUnavailable;

    float sharpness_value = sharpness.has_value() ? sharpness.value() : 0.0f;

    GeoExtent cropIntersection;

    // if we are cropping, and the key doesn't intersect the crop extent, bail out.
    // NOTE: this check happend in intersects(), so it might make more sense to 
    // cache the cropInLocalSRS extent instead.
    if (crop.has_value() && crop.value().valid())
    {
        auto cropInKeySRS = crop.value().transform(key.profile.srs());
        if (cropInKeySRS.valid())
        {
            cropIntersection = cropInKeySRS.intersectionSameSRS(key.extent());
            if (!cropIntersection.valid())
            {
                return Failure_ResourceUnavailable;
            }
        }
    }

    // if this layer has no profile, just go straight to the driver.
    if (!profile.valid())
    {
        result = createImageImplementation_internal(key, io);
    }

    else if (key.profile == profile)
    {
        result = createImageImplementation_internal(key, io);
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto image = assembleImage(key, io);

        if (image)
        {
            result = GeoImage(image, key.extent());

            // automatically re-sharpen a reprojected image to account for quality loss.
            // Do we like this idea?
            if (sharpness_value == 0.0f)
                sharpness_value = 2.0f;
        }
    }

    if (result.ok())
    {
        ROCKY_SOFT_ASSERT(result.value().image());

        if (cropIntersection.valid())
        {
            auto b = key.extent().bounds();
            if (!cropIntersection.contains(b))
            {
                int s0, t0, s1, t1;
                result.value().getPixel(cropIntersection.xmin(), cropIntersection.ymin(), s0, t0);
                result.value().getPixel(cropIntersection.xmax(), cropIntersection.ymax(), s1, t1);
                s0 = clamp(s0, 0, (int)result.value().image()->width() - 1);
                s1 = clamp(s1, 0, (int)result.value().image()->width() - 1);
                t0 = clamp(t0, 0, (int)result.value().image()->height() - 1);
                t1 = clamp(t1, 0, (int)result.value().image()->height() - 1);

                auto image = result.value().image();

                image->eachPixel([&](auto& i)
                    {
                        if ((int)i.s() < s0 || (int)i.s() > s1 || (int)i.t() < t0 || (int)i.t() > t1)
                        {
                            image->write({ 0,0,0,0 }, i);
                        }
                    }
                );
            }
        }

        if (sharpness_value > 0.0f)
        {
            result = GeoImage(result.value().image()->sharpen(sharpness_value), key.extent());
        }
    }

    return result;
}

std::shared_ptr<Image>
ImageLayer::assembleImage(const TileKey& key, const IOOptions& io) const
{
    std::shared_ptr<Mosaic> output;

    // Find the set of keys that covers the same area as in input key in our target profile.
    auto localKeys = key.intersectingKeys(profile);

    // collect raster data for each intersecting key, falling back on ancestor images
    // if none are available at the target LOD.
    std::vector<GeoImage> sources;

    if (localKeys.size() > 0)
    {
        bool hasAtLeastOneSourceAtTargetLOD = false;

        for (auto& localKey : localKeys)
        {
            // first try the weak dependency cache.
            auto cached = _dependencyCache->get(localKey);
            auto cached_value = cached.value.lock();
            if (cached_value)
            {
                sources.emplace_back(cached_value, cached.valueKey.extent());

                if (cached.valueKey.level == localKey.level)
                {
                    hasAtLeastOneSourceAtTargetLOD = true;
                }
            }

            else
            {
                // create the sub tile, falling back until we get real data
                TileKey subKey = localKey;
                Result<GeoImage> subTile = Failure{};
                while (subKey.valid() && !subTile.ok())
                {
                    subTile = createImageImplementation_internal(subKey, io);

                    if (subTile.failed() || !subTile.value().image())
                        subKey.makeParent();

                    if (io.canceled())
                        return {};
                }

                if (subTile.ok() && subTile.value().image())
                {
                    // save it in the weak cache:
                    _dependencyCache->put(localKey, subKey, subTile.value().image());

                    // add it to our sources collection:
                    sources.emplace_back(subTile.value());

                    if (subKey.level == localKey.level)
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
            unsigned layers = 1;

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
                layers = std::max(layers, source.image()->depth());
            }

            // assume all tiles to mosaic are in the same SRS.
            SRSOperation xform = key.extent().srs().to(sources[0].srs());

            // new output:
            output = Mosaic::create(sources[0].image()->pixelFormat(), cols, rows, layers);

            // Cache pointers to the source images that mosaic to create this tile.
            output->dependencies.reserve(sources.size());
            for (auto& source : sources)
                output->dependencies.push_back(source.image());

            // Clean up orphaned entries any time a tile destructs.
            output->cleanupOperation = [captured{ std::weak_ptr(_dependencyCache) }, key]() {
                if (auto cache = captured.lock())
                    cache->clean();
                };

            // Working set of points. it's much faster to xform an entire vector all at once.
            std::vector<glm::dvec3> points;
            points.resize(cols * rows);

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

            // Working bounds of the SRS itself so we can clamp out-of-bounds points.
            // This is especially important when going from Mercator to Geographic
            // where there's no data beyond +/- 85 degrees.
            auto keyExtentInSourceSRS = key.extent().transform(sources[0].srs());

            // transform the sample points to the SRS of our source data tiles:
            if (xform.valid())
            {
                xform.transformArray(&points[0], points.size());

                // clamp the transformed points to the profile SRS.
                if (keyExtentInSourceSRS.valid())
                {
                    keyExtentInSourceSRS.clamp(points.begin(), points.end());
                }
            }

            // Mosaic our sources into a single output image.
            glm::fvec4 pixel;
            for (unsigned layer = 0; layer < layers; ++layer)
            {
                for (unsigned r = 0; r < rows; ++r)
                {
                    for (unsigned c = 0; c < cols; ++c)
                    {
                        unsigned i = r * cols + c;

                        // check each source (high to low LOD) until we get a valid pixel.
                        pixel = { 0,0,0,0 };

                        for (unsigned k = 0; k < sources.size(); ++k)
                        {
                            if (layer < sources[k].image()->depth())
                            {
                                if (sources[k].read(pixel, points[i].x, points[i].y, layer) && pixel.a > 0.0f)
                                {
                                    break;
                                }
                            }
                        }

                        output->write(pixel, c, r, layer);
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
