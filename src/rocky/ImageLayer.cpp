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
    conf.get("shared", _shared);
    conf.get("coverage", _coverage);
    conf.get("accept_draping", _acceptDraping);
    conf.get("transparent_color", _transparentColor);
    conf.get("texture_compression", _textureCompression);
    conf.get("async", _async);

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
    conf.set("shared", _shared);
    conf.set("coverage", _coverage);
    conf.set("transparent_color", _transparentColor);
    conf.set("texture_compression", _textureCompression);
    return conf;
}

void
ImageLayer::setShared(bool value)
{
    setOptionThatRequiresReopen(_shared, value);
}

bool
ImageLayer::getShared() const
{
    return _shared;
}

void
ImageLayer::setCoverage(bool value)
{
    setOptionThatRequiresReopen(_coverage, value);
}

bool
ImageLayer::getCoverage() const
{
    return _coverage;
}

void
ImageLayer::setAsyncLoading(bool value)
{
    _async = value;
}

bool
ImageLayer::getAsyncLoading() const
{
    return _async;
}

Status
ImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = TileLayer::openImplementation(io);
    if (parent.failed())
        return parent;

    //if (!_emptyImage.valid())
    //    _emptyImage = ImageUtils::createEmptyImage();

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
                TileKey bestKey = getBestAvailableTileKey(key);
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
ImageLayer::createImage(
    const GeoImage& canvas,
    const TileKey& key,
    const IOOptions& io)
{
    std::shared_lock lock(layerMutex());
    return createImageImplementation(canvas, key, io);
}

Result<GeoImage>
ImageLayer::createImageInKeyProfile(
    const TileKey& key,
    const IOOptions& io) const
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
        std::shared_lock lock(layerMutex());
        return createImageImplementation(key, io);
    }

    else if (key.profile() == profile())
    {
        // test for an upsampling request:
        bool createUpsampledImage = false;
        if (upsample() == true && maxDataLevel() > key.levelOfDetail())
        {
            TileKey best = getBestAvailableTileKey(key, false);
            if (best.valid())
            {
                TileKey best_upsampled = getBestAvailableTileKey(key, true);
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
            std::shared_lock lock(layerMutex());
            result = createImageImplementation(key, io);
        }
    }
    else
    {
        // If the profiles are different, use a compositing method to assemble the tile.
        result = assembleImage(key, io);
    }

    return result;
}

Result<GeoImage>
ImageLayer::assembleImage(const TileKey& key, const IOOptions& io) const
{
    // If we got here, asset that there's a non-null layer profile.
    if (!profile().valid())
    {
        setStatus(Status(Status::AssertionFailure, "assembleImage with undefined profile"));
        return Result(GeoImage::INVALID);
    }

    GeoImage mosaicedImage;
    Result<GeoImage> result;

    // Get a set of layer tiles that intersect the requested extent.
    std::vector<TileKey> intersectingKeys;
    key.getIntersectingKeys(profile(), intersectingKeys);

    if ( intersectingKeys.size() > 0 )
    {
#if 0
        GeoExtent ee = key.extent().transform(intersectingKeys.front().profile().srs());
        ROCKY_INFO << "Tile " << key.str() << " ... " << ee.toString() << std::endl;
        for (auto key : intersectingKeys) {
            ROCKY_INFO << " - " << key.str() << " ... " << key.extent().toString() << std::endl;
        }
#endif
        double dst_minx, dst_miny, dst_maxx, dst_maxy;
        key.extent().getBounds(dst_minx, dst_miny, dst_maxx, dst_maxy);

        // if we find at least one "real" tile in the mosaic, then the whole result tile is
        // "real" (i.e. not a fallback tile)
        bool retry = false;
        util::ImageMosaic mosaic;

        // keep track of failed tiles.
        std::vector<TileKey> failedKeys;

        for(auto& k : intersectingKeys)
        {
            auto geoimage = createImageInKeyProfile(k, io);

            if (geoimage->valid())
            {
#if 0
                // use std::dynamic_pointer_cast....
                if (dynamic_cast<const TimeSeriesImage*>(image.getImage()))
                {
                    ROCKY_WARN << LC << "Cannot mosaic a TimeSeriesImage. Discarding." << std::endl;
                    return GeoImage::INVALID;
                }
                else if (dynamic_cast<const osg::ImageStream*>(image.getImage()))
                {
                    ROCKY_WARN << LC << "Cannot mosaic an osg::ImageStream. Discarding." << std::endl;
                    return GeoImage::INVALID;
                }
                else
#endif
                {
                    mosaic.getImages().emplace_back(geoimage->image(), k);
                }
            }
            else
            {
                // the tile source did not return a tile, so make a note of it.
                failedKeys.push_back(k);

                if (io.canceled())
                {
                    retry = true;
                    break;
                }
            }
        }

        // Fail is: a) we got no data and the LOD is greater than zero; or
        // b) the operation was canceled mid-stream.
        if ( (mosaic.getImages().empty() && key.levelOfDetail() > 0) || retry)
        {
            // if we didn't get any data at LOD>0, fail.
            return Result(GeoImage::INVALID);
        }

        // We got at least one good tile, OR we got nothing but since the LOD==0 we have to
        // fall back on a lower resolution.
        // So now we go through the failed keys and try to fall back on lower resolution data
        // to fill in the gaps. The entire mosaic must be populated or this qualifies as a bad tile.
        for(auto& k : failedKeys)
        {
            Result<GeoImage> geoimage;

            for(TileKey parentKey = k.createParentKey();
                parentKey.valid() && !geoimage.value.valid();
                parentKey.makeParent())
            {
                {
                    std::shared_lock lock(layerMutex());
                    geoimage = createImageImplementation(parentKey, io);
                }

                if (geoimage.value.valid())
                {
                    Result<GeoImage> cropped;

                    if ( !isCoverage() )
                    {
                        cropped = geoimage.value.crop(
                            k.extent(),
                            false,
                            geoimage->image()->width(),
                            geoimage->image()->height() );
                    }

                    else
                    {
                        // TODO: may not work.... test; tilekey extent will <> cropped extent
                        cropped = geoimage.value.crop(
                            k.extent(),
                            true,
                            geoimage->image()->width(),
                            geoimage->image()->height(),
                            false );
                    }

                    if (cropped.status.ok())
                    {
                        // and queue it.
                        mosaic.getImages().emplace_back(cropped->image(), k);
                    }

                }
            }

            if (!geoimage.value.valid())
            {
                // a tile completely failed, even with fallback. Eject.
                // let it go. The empty areas will be filled with alpha by ImageMosaic.
            }
        }

        // all set. Mosaic all the images together.
        double rxmin, rymin, rxmax, rymax;
        mosaic.getExtents( rxmin, rymin, rxmax, rymax );

        mosaicedImage = GeoImage(
            mosaic.createImage(),
            GeoExtent( profile().srs(), rxmin, rymin, rxmax, rymax ) );
    }
    else
    {
        //ROCKY_DEBUG << "assembleImage: no intersections (" << key.str() << ")" << std::endl;
    }

    // Final step: transform the mosaic into the requesting key's extent.
    if ( mosaicedImage.valid() )
    {
        // GeoImage::reproject() will automatically crop the image to the correct extents.
        // so there is no need to crop after reprojection. Also note that if the SRS's are the
        // same (even though extents are different), then this operation is technically not a
        // reprojection but merely a resampling.

        const GeoExtent& extent = key.extent();

        result = mosaicedImage.reproject(
            key.profile().srs(),
            &extent,
            tileSize(), tileSize(),
            true);
    }

    if (io.canceled())
    {
        return Status(Status::ResourceUnavailable, "Canceled");
    }

    return result;
}

Status
ImageLayer::writeImage(const TileKey& key, const Image* image, const IOOptions& io)
{
    if (status().failed())
        return status();

    std::shared_lock lock(layerMutex());
    return writeImageImplementation(key, image, io);
}

Status
ImageLayer::writeImageImplementation(const TileKey& key, const Image* image, const IOOptions& io) const
{
    return Status(Status::ServiceUnavailable);
}

const std::string
ImageLayer::getCompressionMethod() const
{
    if (isCoverage())
        return "none";

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
