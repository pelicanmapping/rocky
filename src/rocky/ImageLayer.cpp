/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once
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
            dependencies(rhs.dependencies)
        {}

        virtual ~CompositeImage()
        {
            cleanupOperation();
        }

        std::vector<std::shared_ptr<Image>> dependencies;
        std::function<void()> cleanupOperation;
    };
}

ImageLayer::ImageLayer() :
    super()
{
    construct({});
}

ImageLayer::ImageLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
ImageLayer::construct(const JSON& conf)
{
    const auto j = parse_json(conf);
    get_to(j, "nodata_image", _noDataImageLocation);
    get_to(j, "transparent_color", _transparentColor);
    get_to(j, "texture_compression", _textureCompression);

    setRenderType(RENDERTYPE_TERRAIN_SURFACE);

    _dependencyCache = std::make_shared<DependencyCache<TileKey, Image>>();
}

JSON
ImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "nodata_image", _noDataImageLocation);
    set(j, "transparent_color", _transparentColor);
    set(j, "texture_compression", _textureCompression);
    return j.dump();
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key) const
{
    return createImage(key, IOOptions());
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
ImageLayer::createImage(const GeoImage& canvas, const TileKey& key, const IOOptions& io)
{
    std::shared_lock lock(layerStateMutex());
    return createImageImplementation(canvas, key, io);
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
    // If the layer is disabled, bail out.
    if ( !isOpen() )
    {
        return Result(GeoImage::INVALID);
    }

    // Make sure the request is in range.
    // TODO: perhaps this should be a call to mayHaveData(key) instead.
    if ( !isKeyInLegalRange(key) )
    {
        return Result(GeoImage::INVALID);
    }

    // Tile gate prevents two threads from requesting the same key
    // at the same time, which would be unnecessary work. Only lock
    // the gate if there is an L2 cache active
    ROCKY_TODO("Sentry");
    //util::ScopedGate<TileKey> scopedGate(_sentry, key, [&]() {
    //    return _memCache.valid();
    //});

    Result<GeoImage> result;

    // if this layer has no profile, just go straight to the driver.
    if (!profile().valid())
    {
        return createImageImplementation_internal(key, io);
    }

    else if (key.profile() == profile())
    {
        result = createImageImplementation_internal(key, io);
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto image = assembleImage(key, io);
        result = GeoImage(image, key.extent());
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
    using KeyedImage = std::pair<TileKey, GeoImage>;
    std::vector<KeyedImage> sources;

    if (intersectingKeys.size() > 0)
    {
        bool hasAtLeastOneSourceAtTargetLOD = false;

        for (auto& intersectingKey : intersectingKeys)
        {
            TileKey subKey = intersectingKey;
            Result<GeoImage> subImage;
            while (subKey.valid() && !subImage.status.ok())
            {
                subImage = createImageImplementation_internal(subKey, io);
                if (subImage.status.failed())
                    subKey.makeParent();

                if (io.canceled())
                    return {};
            }

            if (subImage.status.ok())
            {
                // got a valid image, so add it to our sources collection:
                sources.emplace_back(subKey, subImage.value);

                if (subKey.levelOfDetail() == targetLOD)
                {
                    hasAtLeastOneSourceAtTargetLOD = true;
                }
            }
        }

        // If we actually got at least one piece of usable data,
        // move ahead and build a mosaic of all sources.
        if (hasAtLeastOneSourceAtTargetLOD)
        {
            unsigned width = 0;
            unsigned height = 0;

            // sort the sources by LOD (highest first).
            std::sort(
                sources.begin(), sources.end(),
                [](const KeyedImage& lhs, const KeyedImage& rhs) {
                    return lhs.first.levelOfDetail() > rhs.first.levelOfDetail();
                });

            // output size is the max of all the source sizes.
            for (auto iter : sources)
            {
                auto& source = iter.second;
                width = std::max(width, source.image()->width());
                height = std::max(height, source.image()->height());
            }

            // assume all tiles to mosaic are in the same SRS.
            SRSOperation xform = key.extent().srs().to(sources[0].second.srs());

            // new output:
            output = CompositeImage::create(Image::R8G8B8A8_UNORM, width, height);

            // Cache pointers to the source images that mosaic to create this image.
            output->dependencies.reserve(sources.size());
            for (auto& source : sources)
                output->dependencies.push_back(source.second.image());

            // Clean up orphaned entries any time a tile destructs.
            output->cleanupOperation = [captured{ std::weak_ptr(_dependencyCache) }, key]() {
                auto cache = captured.lock();
                if (cache)
                    cache->clean();
                };

            // Working set of points. it's much faster to xform an entire vector all at once.
            std::vector<glm::dvec3> points;
            points.assign(width * height, { 0, 0, 0 });

            double minx, miny, maxx, maxy;
            key.extent().getBounds(minx, miny, maxx, maxy);
            double dx = (maxx - minx) / (double)(width - 1);
            double dy = (maxy - miny) / (double)(height - 1);

            // build a grid of sample points:
            for (unsigned r = 0; r < height; ++r)
            {
                double y = miny + (dy * (double)r);
                for (unsigned c = 0; c < width; ++c)
                {
                    double x = minx + (dx * (double)c);
                    points[r * width + c].x = x;
                    points[r * width + c].y = y;
                }
            }

            // transform the sample points to the SRS of our source data tiles:
            if (xform.valid())
            {
                xform.transformArray(&points[0], points.size());
            }

            // Mosaic our sources into a single output image.
            for (unsigned r = 0; r < height; ++r)
            {
                double y = miny + (dy * (double)r);

                for (unsigned int c = 0; c < width; ++c)
                {
                    double x = minx + (dx * (double)c);

                    unsigned i = r * width + c;

                    // For each sample point, try each heightfield.  The first one with a valid elevation wins.
                    glm::fvec4 pixel(0, 0, 0, 0);

                    // sources are ordered from low to high LOD, so iterater backwards.
                    for (unsigned k = 0; k < sources.size(); ++k)
                    {
                        auto& image = sources[k].second;

                        // get the elevation value, at the same time transforming it vertically into the
                        // requesting key's vertical datum.
                        if (image.read(pixel, points[i].x, points[i].y) && pixel.a > 0.0f)
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

Status
ImageLayer::writeImage(const TileKey& key, shared_ptr<Image> image, const IOOptions& io)
{
    if (status().failed())
        return status();

    std::shared_lock lock(layerStateMutex());
    return writeImageImplementation(key, image, io);
}

Status
ImageLayer::writeImageImplementation(const TileKey& key, shared_ptr<Image> image, const IOOptions& io) const
{
    return Status(Status::ServiceUnavailable);
}

const std::string
ImageLayer::getCompressionMethod() const
{
    return _textureCompression;
}

void
ImageLayer::modifyTileBoundingBox(const TileKey& key, Box& box) const
{
    //if (_altitude.has_value())
    //{
    //    if (_altitude->as(Units::METERS) > box.zmax)
    //    {
    //        box.zmax = _altitude->as(Units::METERS);
    //    }
    //}
    super::modifyTileBoundingBox(key, box);
}
