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
            return false;
        }
        if (hf->width() < 1 || hf->width() > 1024) {
            return false;
        }

        return true;
    }
}


namespace
{
    class HeightfieldMosaic : public Inherit<Heightfield, HeightfieldMosaic>
    {
    public:
        HeightfieldMosaic(unsigned s, unsigned t) : super(s, t) { }
        HeightfieldMosaic(const HeightfieldMosaic& rhs) = default;

        std::shared_ptr<Image> clone() const override
        {
            return std::make_shared<HeightfieldMosaic>(*this);
        }

        std::vector<std::shared_ptr<Heightfield>> dependencies;
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

    // Disable max-level support for elevation data because it makes no sense.
    maxLevel.clear();
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

    return ResultVoidOK;
}

void
ElevationLayer::closeImplementation()
{
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
    auto localKeys = key.intersectingKeys(profile);

    // collect heightfield for each intersecting key. Note, we're hitting the
    // underlying tile source here, so there's no vetical datum shifts happening yet.
    // we will do that later.
    std::vector<GeoHeightfield> sources;

    if (localKeys.size() > 0)
    {
        auto keyExtent = key.extent();
        unsigned numSourcesAtFullResolution = 0;

        for (auto& localKey : localKeys)
        {
            // resolve an image for each local key.
            GeoHeightfield localTile;
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
                    auto hf = std::dynamic_pointer_cast<Heightfield>(cached.value().first);
                    localTile = GeoHeightfield(hf, cached.value().second);
                    fromResidentCache = true;
                }
            }

            // not found in the resident cache; go to the source, and fall back until we get
            // a usable image tile.
            while (!localTile.valid() && actualKey.valid())
            {
                auto r = createHeightfieldImplementation_internal(actualKey, io);

                if (io.canceled())
                    return {};
                else if (r.ok() && r.value().heightfield())
                    localTile = std::move(r.value());
                else
                    actualKey.makeParent();
            }

            if (localTile.valid())
            {
                // if the image came from the source, register it with the resident image cache
                if (!fromResidentCache && io.services().residentImageCache)
                {
                    io.services().residentImageCache->put(cacheKey, localTile.heightfield(), actualKey.extent());
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
                output->dependencies.emplace_back(source.heightfield());

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
                    float& height = output->heightAt(c, r);

                    for (unsigned i = 0; i < sources.size(); ++i)
                    {
                        unsigned j = useIndirectIndexing ? indexes[i] : i;
                        height = sources[j].heightAtLocation(point.x, point.y, Interpolation::Bilinear);
                        if (height != NO_DATA_VALUE) {
                            std::swap(indexes[i], indexes[0]); // move the used source to the front
                            break;
                        }
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
            auto cacheKey = key.str() + '-' + std::to_string(uid()) + "-" + std::to_string(revision());

            auto cached = io.services().residentImageCache->get(cacheKey);
            if (cached.has_value())
            {
                auto hf = std::dynamic_pointer_cast<Heightfield>(cached.value().first);
                return GeoHeightfield(hf, cached.value().second);
            }

            auto r = createHeightfieldInKeyProfile(key, io);

            if (r.ok())
            {
                io.services().residentImageCache->put(cacheKey, r.value().heightfield(), key.extent());
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
    Result<GeoHeightfield> result = Failure_ResourceUnavailable;

    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), result);

    // Check that the key is legal (in valid LOD range, etc.)
    if (!intersects(key))
        return Failure_ResourceUnavailable;

    // we check the srs too in case the vdatums differ.
    if (key.profile == profile && key.profile.srs() == profile.srs())
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

    if (result.ok())
    {
        auto hf = result.value().heightfield();

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
    }

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
