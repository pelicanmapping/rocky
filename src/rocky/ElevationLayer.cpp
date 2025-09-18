/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ElevationLayer.h"
#include "Heightfield.h"
#include "json.h"

#include <cinttypes>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[ElevationLayer] \"" << name().value() << "\" : "

namespace
{
    // perform very basic sanity-check validation on a heightfield.
    bool validateHeightfield(const Image* image)
    {
        if (!image)
            return false;
        if (image->height() < 1 || image->height() > 1024)
            return false;
        if (image->width() < 1 || image->width() > 1024)
            return false;
        return true;
    }
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
    get_to(j, "noDataValue", noDataValue);
    get_to(j, "minValidValue", minValidValue);
    get_to(j, "maxValidValue", maxValidValue);
    std::string encoding_value;
    if (get_to(j, "encoding", encoding_value))
    {
        if (encoding_value == "singleChannel")
            encoding = Encoding::SingleChannel;
        else if (encoding_value == "mapbox")
            encoding = Encoding::MapboxRGB;
        else if (encoding_value == "terrarium")
            encoding = Encoding::TerrariumRGB;
    }

    // Disable max-level support for elevation data because it makes no sense.
    maxLevel.clear();
}

std::string
ElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "offset", offset);
    set(j, "noDataValue", noDataValue);
    set(j, "minValidValue", minValidValue);
    set(j, "maxValidValue", maxValidValue);
    if (encoding.has_value(Encoding::SingleChannel))
        set(j, "encoding", "singleChannel");
    else if (encoding.has_value(Encoding::MapboxRGB))
        set(j, "encoding", "mapbox");
    else if (encoding.has_value(Encoding::TerrariumRGB))
        set(j, "encoding", "terrarium");

    return j.dump();
}

Result<>
ElevationLayer::openImplementation(const IOOptions& io)
{
    auto r = super::openImplementation(io);
    if (r.failed())
        return r;

    return ResultVoidOK;
}

void
ElevationLayer::closeImplementation()
{
    super::closeImplementation();
}

void
ElevationLayer::normalizeNoDataValues(Image* hf) const
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

std::shared_ptr<Image>
ElevationLayer::assembleTile(const TileKey& key, const IOOptions& io) const
{
    std::shared_ptr<Mosaic> output;

    // Determine the intersecting keys
    auto localKeys = key.intersectingKeys(profile);

    // collect heightfield for each intersecting key. Note, we're hitting the
    // underlying tile source here, so there's no vetical datum shifts happening yet.
    // we will do that later.
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
                        auto r = createTileImplementation_internal(actualKey, io);

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

            // output size is the max of all the source sizes.
            for (auto& source : sources)
            {
                cols = std::max(cols, source.image()->width());
                rows = std::max(rows, source.image()->height());
            }

            // assume all tiles to mosaic are in the same SRS.
            SRSOperation xform = key.extent().srs().to(sources[0].srs());

            // Now sort the heightfields by resolution to make sure we're sampling
            // the highest resolution one first.
            std::sort(
                sources.begin(), sources.end(),
                [](const GeoImage& lhs, const GeoImage& rhs) {
                    return lhs.extent().width() < rhs.extent().width();
                });

            // new output HF:
            output = Mosaic::create(sources[0].image()->pixelFormat(), cols, rows);
            Heightfield hf(output);
            hf.fill(NO_DATA_VALUE);

            // Cache pointers to the source images that mosaic to create this tile.
            output->dependencies.reserve(sources.size());
            for (auto& source : sources)
                output->dependencies.emplace_back(source.image());

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

            // indirect indexing is a trick that minimized the number of sources we
            // need to iterate over, but we can only use it when all resolutions are
            // the same.
            std::vector<unsigned> indexes(sources.size());
            std::iota(indexes.begin(), indexes.end(), 0);
            bool useIndirectIndexing = (numSourcesAtFullResolution == sources.size());

            // sample the heights:
            for (unsigned r = 0; r < rows; ++r)
            {
                for (unsigned c = 0; c < cols; ++c)
                {
                    auto& point = points[r * cols + c];
                    float& height = hf.heightAt(c, r);

                    for (unsigned i = 0; i < sources.size(); ++i)
                    {
                        unsigned j = useIndirectIndexing ? indexes[i] : i;

                        auto r = sources[j].read(point.x, point.y);
                        height = r.ok() ? r.value().r : NO_DATA_VALUE;

                        if (height != NO_DATA_VALUE)
                        {
                            std::swap(indexes[i], indexes[0]);
                            break;
                        }
                    }

                    if (height != NO_DATA_VALUE)
                    {
                        height -= point.z; // apply reverse vdatum offset
                    }
                }

                if (io.canceled())
                    return {};
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
ElevationLayer::createTile(const TileKey& key, const IOOptions& io) const
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
ElevationLayer::createTileImplementation_internal(const TileKey& key, const IOOptions& io) const
{
    auto result = createTileImplementation(key, io);

    if (result.ok())
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(result.value().valid(), result);

        if (result.value().image()->pixelFormat() != HEIGHTFIELD_FORMAT)
        {
            // If the image is not a heightfield, decode it to a heightfield.
            auto hf = decodeRGB(result.value().image());
            if (hf)
            {
                ROCKY_SOFT_ASSERT_AND_RETURN(hf->pixelFormat() == HEIGHTFIELD_FORMAT, result);
                result = GeoImage(hf, key.extent());
            }
            else
            {
                result = Failure(Failure::GeneralError, "Failed to decode RGB image to heightfield.");
            }
        }
    }

    else
    {
        Log()->debug("Failed to create heightfield for key {0} : {1}", key.str(), result.error().message);
    }

    return result;
}

Result<GeoImage>
ElevationLayer::createTileInKeyProfile(const TileKey& key, const IOOptions& io) const
{
    Result<GeoImage> result = Failure_ResourceUnavailable;

    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), result);

    // Check that the key is legal (in valid LOD range, etc.)
    if (!intersects(key))
        return Failure_ResourceUnavailable;

    // we check the srs too in case the vdatums differ.
    if (key.profile == profile && key.profile.srs() == profile.srs())
    {
        auto r = createTileImplementation_internal(key, io);

        if (r.failed())
            return r;
        else
            result = r.value();
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        auto hf = assembleTile(key, io);
        if (hf)
        {
            result = GeoImage(hf, key.extent());
        }
    }

    // Check for cancelation before writing to a cache
    if (io.canceled())
    {
        return Failure_ResourceUnavailable;
    }

    if (result.ok())
    {
        auto hf = result.value().image();

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

        result = GeoImage(hf, key.extent());
    }

    return result;
}

std::shared_ptr<Image>
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
    Heightfield hf(view.width(), view.height());

    glm::fvec4 pixel;

    if (encoding == Encoding::TerrariumRGB)
    {
        view.eachPixel([&](auto& i)
            {
                pixel = view.read(i.s(), i.t());

                float height = (pixel.r * 255.0f * 256.0f + pixel.g * 255.0f + pixel.b * 255.0f / 256.0f) - 32768.0f;
                
                if (height < -9999 || height > 999999)
                    height = NO_DATA_VALUE;

                hf.heightAt(i.s(), i.t()) = height;
            });
    }

    else // default to MapboxRGB
    {
        view.eachPixel([&](auto& i)
            {
                pixel = view.read(i.s(), i.t());

                float height = -10000.f + ((pixel.r * 256.0f * 256.0f + pixel.g * 256.0f + pixel.b) * 256.0f * 0.1f);

                if (height < -9999 || height > 999999)
                    height = NO_DATA_VALUE;

                hf.heightAt(i.s(), i.t()) = height;
            });
    }

    return hf.image;
}
