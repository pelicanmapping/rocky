/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "ImageLayer.h"

#include "IOTypes.h"
#include "GeoImage.h"
#include "Image.h"
#include "TileKey.h"
#include "json.h"

#include <cinttypes>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;


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
    get_to(j, "noDataColor", noDataColor);
}

std::string
ImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "sharpness", sharpness);
    set(j, "crop", crop);
    set(j, "noDataColor", noDataColor);
    return j.dump();
}

Result<>
ImageLayer::openImplementation(const IOOptions& io)
{
    auto r = super::openImplementation(io);
    if (r.failed())
        return r;

    return ResultVoidOK;
}

void
ImageLayer::closeImplementation()
{
    super::closeImplementation();
}

Result<GeoImage>
ImageLayer::createTile(const TileKey& key, const IOOptions& io) const
{
    // lock prevents closing the layer while creating an image
    std::shared_lock readLock(layerStateMutex());

    if (status().failed())
        return status().error();

    return getOrCreateTile(key, io, [&]()
        {
            return createTileInKeyProfile(key, io);
        });
}

Result<GeoImage>
ImageLayer::createTileInKeyProfile(const TileKey& key, const IOOptions& io) const
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
        result = invokeCreateTileImplementation(key, io);
    }

    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto image = assembleTile(key, io);

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
ImageLayer::assembleTile(const TileKey& key, const IOOptions& io) const
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
            TileKey actualKey = localKey;

            auto create = [&]() -> Result<GeoImage>
                {
                    // not found in the resident cache; go to the source, and fall back until we get
                    // a usable image tile.
                    while (actualKey.valid())
                    {
                        auto r = invokeCreateTileImplementation(actualKey, io);

                        if (io.canceled())
                            return Failure_OperationCanceled;
                        else if (r.ok() && r.value().image())
                            return r;
                        else
                            actualKey.makeParent();
                    }

                    return Failure_ResourceUnavailable;
                };

            auto localTile = getOrCreateTile(localKey, io, create);

            if (localTile.ok())
            {
                sources.emplace_back(std::move(localTile.value()));

                if (actualKey.level == localKey.level)
                {
                    ++numSourcesAtFullResolution;
                }
            }

            if (io.canceled())
                return {};
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
            glm::fvec4 emptypixel(0.0f, 0.0f, 0.0f, 0.0f);

            for (unsigned layer = 0; layer < layers; ++layer)
            {
                for (unsigned r = 0; r < rows; ++r)
                {
                    for (unsigned c = 0; c < cols; ++c)
                    {
                        unsigned i = r * cols + c;

                        // check each source (high to low resolution) until we get a valid pixel.
                        bool wrote = false;

                        for(unsigned n = 0; n < indexes.size(); ++n)
                        {
                            unsigned k = useIndirectIndexing ? indexes[n] : n;

                            if (layer < sources[k].image()->depth())
                            {
                                auto pixel = sources[k].read(points[i].x, points[i].y, layer);
                                if (pixel.ok() && pixel.value().a > 0.0f)
                                {
                                    output->write(pixel.value(), c, r, layer);
                                    wrote = true;
                                    std::swap(indexes[n], indexes[0]);
                                    break;
                                }
                            }
                        }

                        if (!wrote)
                        {
                            output->write(emptypixel, c, r, layer);
                        }
                    }

                    if (io.canceled())
                        return {};
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

Result<GeoImage>
ImageLayer::invokeCreateTileImplementation(const TileKey& key, const IOOptions& io) const
{
    auto r = createTileImplementation(key, io);

    if (r.ok())
    {
        auto image = r.value().image();

        // check the no-data color.
        if (noDataColor.has_value() && image->width() > 0 && image->height() > 0)
        {
            auto view_format =
                image->pixelFormat() == Image::R8_SRGB ? Image::R8_UNORM :
                image->pixelFormat() == Image::R8G8_SRGB ? Image::R8G8_UNORM :
                image->pixelFormat() == Image::R8G8B8_SRGB ? Image::R8G8B8_UNORM :
                image->pixelFormat() == Image::R8G8B8A8_SRGB ? Image::R8G8B8A8_UNORM :
                image->pixelFormat();

            Image i = image->viewAs(view_format);

            if (glm::all(glm::epsilonEqual(i.read(0, 0), noDataColor.value(), 1e-3f)) &&
                glm::all(glm::epsilonEqual(i.read(i.width()-1, i.height()-1), noDataColor.value(), 1e-3f)))
            {
                return Failure_ResourceUnavailable;
            }
        }
    }

    return r;
}