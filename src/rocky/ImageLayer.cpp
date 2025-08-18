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
    // This in combination with the resident image cache speeds up assemble operations.
    class Mosaic : public Inherit<Image, Mosaic>
    {
    public:
        Mosaic(PixelFormat format, unsigned s, unsigned t, unsigned r = 1) :
            super(format, s, t, r) { }

        Mosaic(const Mosaic& rhs) = default;

        std::shared_ptr<Image> clone() const override
        {
            return std::make_shared<Mosaic>(*this);
        }

        std::vector<std::shared_ptr<Image>> dependencies;
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

#if 0
    _dependencyCache = std::make_shared<TileMosaicWeakCache<Image>>();
#endif

    return ResultVoidOK;
}

void
ImageLayer::closeImplementation()
{
#if 0
    _dependencyCache = nullptr;
#endif

    super::closeImplementation();
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key, const IOOptions& io) const
{
    // lock prevents closing the layer while creating an image
    std::shared_lock readLock(layerStateMutex());

    if (status().ok())
    {
        if (io.services().residentImageCache)
        {
            auto cacheKey = key.str() + '-' + std::to_string(key.profile.hash()) + '-' + std::to_string(uid()) + "-" + std::to_string(revision());

            auto cached = io.services().residentImageCache->get(cacheKey);
            if (cached.has_value())
                return GeoImage(cached.value().first, cached.value().second);

            auto r = createImageInKeyProfile(key, io);

            if (r.ok())
            {
                io.services().residentImageCache->put(cacheKey, r.value().image(), r.value().extent());
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
    return createImageImplementation(key, io);
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
    if (!profile.valid() || (key.profile == profile))
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
        auto keyExtent = key.extent();
        unsigned numSourcesAtFullResolution = 0;

        for (auto& localKey : localKeys)
        {
            // resolve an image for each local key.
            GeoImage localTile;
            TileKey actualKey = localKey;

            // check the resident image cache first.
            // this cache keeps a weak pointer to any image anywhere in memory; 
            // this speeds up mosacing a lot during reprojection.
            auto cacheKey = localKey.str() + '-' + std::to_string(localKey.profile.hash()) + '-' + std::to_string(uid()) + '-' + std::to_string(revision());
            bool fromResidentCache = false;

            if (io.services().residentImageCache)
            {
                auto cached = io.services().residentImageCache->get(cacheKey);
                if (cached.has_value())
                {
                    localTile = GeoImage(cached.value().first, cached.value().second);
                    fromResidentCache = true;
                }
            }

            // not found in the resident cache; go to the source, and fall back until we get
            // a usable image tile.
            while (!localTile.valid() && actualKey.valid())
            {
                auto r = createImageImplementation_internal(actualKey, io);

                if (io.canceled())
                    return {};
                else if (r.ok() && r.value().image())
                    localTile = std::move(r.value());
                else
                    actualKey.makeParent();
            }

            if (localTile.valid())
            {
                // if the image came from the source, register it with the resident image cache
                if (!fromResidentCache && io.services().residentImageCache)
                {
                    io.services().residentImageCache->put(cacheKey, localTile.image(), actualKey.extent());
                }

                sources.emplace_back(std::move(localTile));

                if (actualKey.level == localKey.level)
                {
                    ++numSourcesAtFullResolution;
                }
            }
        }

        // If we actually got at least one piece of usable data,
        // move ahead and build a mosaic of all sources.
        if (numSourcesAtFullResolution > 0)
        {
            unsigned cols = 0;
            unsigned rows = 0;
            unsigned layers = 1;

            // sort the sources by resolution (highest first)
            bool useIndirectIndexing = true;

            if (numSourcesAtFullResolution < sources.size())
            {
                useIndirectIndexing = false;

                std::sort(
                    sources.begin(), sources.end(),
                    [](const GeoImage& lhs, const GeoImage& rhs) {
                        return lhs.extent().width() < rhs.extent().width();
                    });
            }

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
                output->dependencies.emplace_back(source.image());

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

            // Indirect indexing lets us do a basic "LRU" cache when looping through multiple images.
            std::vector<unsigned> indexes(sources.size());
            std::iota(indexes.begin(), indexes.end(), 0);

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

                        for(unsigned n = 0; n < indexes.size(); ++n)
                        {
                            unsigned k = useIndirectIndexing ? indexes[n] : n;

                            if (layer < sources[k].image()->depth())
                            {
                                if (sources[k].read(pixel, points[i].x, points[i].y, layer) && pixel.a > 0.0f)
                                {
                                    std::swap(indexes[n], indexes[0]);
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
