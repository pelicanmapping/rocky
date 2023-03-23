/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ImageMosaic.h"
#include "Image.h"
#include "TileKey.h"
#include "Instance.h"

#define LC "[ImageMosaic] "

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;


/***************************************************************************/

ImageMosaic::SourceImage::SourceImage(shared_ptr<Image> image_, const TileKey& key)
{
    image = image_;
    key.extent().getBounds(xmin, ymin, xmax, ymax);
    tilex = key.tileX();
    tiley = key.tileY();
}

void ImageMosaic::getExtents(double &minX, double &minY, double &maxX, double &maxY)
{
    minX = DBL_MAX;
    maxX = -DBL_MAX;
    minY = DBL_MAX;
    maxY = -DBL_MAX;

    for(auto& tile : _images)
    {
        minX = std::min(tile.xmin, minX);
        minY = std::min(tile.ymin, minY);
        maxX = std::max(tile.xmax, maxX);
        maxY = std::max(tile.ymax, maxY);
    }
}

shared_ptr<Image>
ImageMosaic::createImage() const
{
    if (_images.size() == 0)
    {
        return 0;
    }

    // find the first valid tile and use its size as the mosaic tile size
    const SourceImage* tile = nullptr;
    for (int i = 0; i<_images.size() && !tile; ++i)
        if (_images[i].image && _images[i].image->valid())
            tile = &_images[i];

    if ( !tile )
        return nullptr;

    unsigned int tileWidth = tile->image->width();
    unsigned int tileHeight = tile->image->height();

    unsigned int minTileX = tile->tilex;
    unsigned int minTileY = tile->tiley;
    unsigned int maxTileX = tile->tilex;
    unsigned int maxTileY = tile->tiley;

    // Compute the tile size.
    for(auto& c : _images)
    {
        if (c.tilex < minTileX) minTileX = c.tilex;
        if (c.tiley < minTileY) minTileY = c.tiley;

        if (c.tilex > maxTileX) maxTileX = c.tilex;
        if (c.tiley > maxTileY) maxTileY = c.tiley;
    }

    unsigned int tilesWide = maxTileX - minTileX + 1;
    unsigned int tilesHigh = maxTileY - minTileY + 1;

    unsigned int pixelsWide = tilesWide * tileWidth;
    unsigned int pixelsHigh = tilesHigh * tileHeight;
    unsigned int tileDepth = tile->image->depth();

    // make the new image and initialize it to transparent-white:
    auto result = Image::create(
        tile->image->pixelFormat(),
        pixelsWide,
        pixelsHigh,
        tileDepth);

    Image::Pixel clear(1, 1, 1, 0);
    result->get_iterator().forEachPixel([&](const Image::iterator& i) { 
        result->write(clear, i.s(), i.t());
        });

    // Composite the incoming images into the master image
    for (auto& comp : _images)
    {
        //Determine the indices in the master image for this image
        if (comp.image)
        {
            int dstX = (comp.tilex - minTileX) * tileWidth;
            int dstY = (maxTileY - comp.tiley) * tileHeight;

            comp.image->copyAsSubImage(result.get(), dstX, dstY);
        }
    }

    return result;
}

/***************************************************************************/
