/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>
#include <vector>

namespace rocky
{
    class Image;
    class TileKey;
}

namespace rocky { namespace util
{
    /**
     * Utility class for extracting a single image from a collection of image tiles
     */
    class ROCKY_EXPORT ImageMosaic
    {
    public:
        struct ROCKY_EXPORT SourceImage
        {
            SourceImage(shared_ptr<Image> image, const TileKey& key);
            shared_ptr<Image> image;
            double xmin, ymin, xmax, ymax;
            unsigned tilex, tiley;
        };

    public:

        shared_ptr<Image> createImage() const;

        using SourceImages = std::vector<SourceImage>;

        SourceImages& getImages() { return _images; }

        void getExtents(double &minX, double &minY, double &maxX, double &maxY);

    protected:
        SourceImages _images;
    };
} }
