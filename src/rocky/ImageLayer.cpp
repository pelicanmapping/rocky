/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "ImageLayer.h"

#include "Color.h"
#include "ImageMosaic.h"
#include "IOTypes.h"
#include "Metrics.h"
#include "Utils.h"
#include "GeoImage.h"
#include "Image.h"
#include "TileKey.h"
#include <cinttypes>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[ImageLayer] \"" << name().value() << "\" "

ImageLayer::ImageLayer() :
    Inherit()
{
    construct(Config());
}

ImageLayer::ImageLayer(const Config& conf) :
    Inherit(conf)
{
    construct(conf);
}

void
ImageLayer::construct(const Config& conf)
{
    conf.get("nodata_image", _noDataImageLocation);
    conf.get("accept_draping", _acceptDraping);
    conf.get("transparent_color", _transparentColor);
    conf.get("texture_compression", _textureCompression);
    conf.get("coverage", _coverage);

    if (_acceptDraping.has_value())
    {
        setAcceptDraping(_acceptDraping);
    }

    setRenderType(RENDERTYPE_TERRAIN_SURFACE);
}

Config
ImageLayer::getConfig() const
{
    Config conf = super::getConfig();
    conf.set("nodata_image", _noDataImageLocation);
    conf.set("transparent_color", _transparentColor);
    conf.set("texture_compression", _textureCompression);
    conf.set("coverage", _coverage);
    return conf;
}

Status
ImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

#if 0
    if (!options().shareTexUniformName().has_value())
        options().shareTexUniformName().init("layer_" + std::to_string(getUID()) + "_tex");

    if (!options().shareTexMatUniformName().has_value() )
        options().shareTexMatUniformName().init(options().shareTexUniformName().get() + "_matrix");
#endif

    return StatusOK;
}


#if 0
void
ImageLayer::setAltitude(const Distance& value)
{
    options().altitude() = value;

    if (value != 0.0)
    {
        osg::StateSet* stateSet = getOrCreateStateSet();

        stateSet->addUniform(
            new osg::Uniform("oe_terrain_altitude", (float)options().altitude()->as(Units::METERS)),
            osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        stateSet->setMode(GL_CULL_FACE, 0);
    }
    else
    {
        osg::StateSet* stateSet = getOrCreateStateSet();
        getOrCreateStateSet()->removeUniform("oe_terrain_altitude");
        stateSet->removeMode(GL_CULL_FACE);
    }
}

const Distance&
ImageLayer::getAltitude() const
{
    return options().altitude().get();
}
#endif

void
ImageLayer::setAcceptDraping(bool value)
{
    _acceptDraping = value;

#if 0
    if (value == true && getStateSet() != nullptr)
        getStateSet()->removeDefine("ROCKY_DISABLE_DRAPING");
    else
        getOrCreateStateSet()->setDefine("ROCKY_DISABLE_DRAPING");
#endif
    ROCKY_TODO("Draping");
}

bool
ImageLayer::getAcceptDraping() const
{
    return _acceptDraping;
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key) const
{
    return createImage(key, IOOptions());
}

Result<GeoImage>
ImageLayer::createImage(const TileKey& key, const IOOptions& io) const
{    
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT(getName() + " " + key.str());

    if (!isOpen())
    {
        return Result(GeoImage::INVALID);
    }

    //NetworkMonitor::ScopedRequestLayer layerRequest(getName());

    auto result = createImageInKeyProfile(key, io);

#if 0
    // Post-cache operations:
    if (!_postLayers.empty())
    {
        for (auto& post : _postLayers)
        {
            if (!result.valid())
            {
                TileKey bestKey = bestAvailableTileKey(key);
                result = createImageInKeyProfile(bestKey, progress);
            }

            result = post->createImage(result, key, progress);
        }
    }

    if (result.valid())
    {
        postCreateImageImplementation(result, key, progress);
    }
#endif

    return result;
}

Result<GeoImage>
ImageLayer::createImage(const GeoImage& canvas, const TileKey& key, const IOOptions& io)
{
    std::shared_lock lock(layerStateMutex());
    return createImageImplementation(canvas, key, io);
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
        std::shared_lock lock(layerStateMutex());
        return createImageImplementation(key, io);
    }

    else if (key.profile() == profile())
    {
        // test for an upsampling request:
        bool createUpsampledImage = false;
        if (upsample() == true && maxDataLevel() > key.levelOfDetail())
        {
            TileKey best = bestAvailableTileKey(key, false);
            if (best.valid())
            {
                TileKey best_upsampled = bestAvailableTileKey(key, true);
                if (best_upsampled.valid() &&
                    best.levelOfDetail() < best_upsampled.levelOfDetail())
                {
                    createUpsampledImage = true;
                }
            }
        }

        if (createUpsampledImage == true)
        {
            ROCKY_TODO("upsampled image");
            //result = createFractalUpsampledImage(key, ioc);
        }
        else
        {
            std::shared_lock lock(layerStateMutex());
            result = createImageImplementation(key, io);
        }
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
    shared_ptr<Image> output;

    // Collect the rasters for each of the intersecting tiles.
    std::vector<GeoImage> source_list;

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
        for (auto& layerKey : intersectingKeys)
        {
            if (isKeyInLegalRange(layerKey))
            {
                std::shared_lock L(layerStateMutex());
                auto result = createImageImplementation(layerKey, io);

                if (result.status.ok() && result.value.valid())
                {
                    source_list.push_back(result.value);
                }
            }
        }

        // If we actually got data, resample/reproject it to match the incoming TileKey's extents.
        if (source_list.size() > 0)
        {
            unsigned width = 0;
            unsigned height = 0;

            auto keyExtent = key.extent();

            std::vector<SRSOperation> xform_list;

            for (auto& source : source_list)
            {
                width = std::max(width, source.image()->width());
                height = std::max(height, source.image()->height());
                xform_list.push_back(keyExtent.srs().to(source.srs()));
            }

            // Now sort the heightfields by resolution to make sure we're sampling
            // the highest resolution one first.
            //std::sort(source_list.begin(), source_list.end(), GeoHeightfield::SortByResolutionFunctor());

            // new output:
            output = Image::create(Image::R8G8B8A8_UNORM, width, height);

            //Go ahead and set up the heightfield so we don't have to worry about it later
            double minx, miny, maxx, maxy;
            key.extent().getBounds(minx, miny, maxx, maxy);
            double dx = (maxx - minx) / (double)(width - 1);
            double dy = (maxy - miny) / (double)(height - 1);

            //Create the new heightfield by sampling all of them.
            for (unsigned int c = 0; c < width; ++c)
            {
                double x = minx + (dx * (double)c);
                for (unsigned r = 0; r < height; ++r)
                {
                    double y = miny + (dy * (double)r);

                    // For each sample point, try each heightfield.  The first one with a valid elevation wins.
                    fvec4 pixel(0, 0, 0, 0);

                    for (unsigned k = 0; k < source_list.size(); ++k)
                    {
                        // get the elevation value, at the same time transforming it vertically into the
                        // requesting key's vertical datum.
                        if (source_list[k].read(pixel, x, y, xform_list[k]) && pixel.a > 0.0f)
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
