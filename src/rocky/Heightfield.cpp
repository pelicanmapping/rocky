/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Heightfield.h"
#include "GeoCommon.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    bool validateSamples(float &a, float &b, float &c, float &d)
    {
        // If ALL the sample points are NO_DATA_VALUE then we can't do anything.
        if (a == NO_DATA_VALUE && b == NO_DATA_VALUE && c == NO_DATA_VALUE && d == NO_DATA_VALUE)
        {
            return false;
        }

        // If any of the samples are valid but some are NO_DATA_VALUE we can replace the nodata with valid values.
        if (a == NO_DATA_VALUE ||
            b == NO_DATA_VALUE ||
            c == NO_DATA_VALUE ||
            d == NO_DATA_VALUE)
        {
            float validValue = a;
            if (validValue == NO_DATA_VALUE) validValue = b;
            if (validValue == NO_DATA_VALUE) validValue = c;
            if (validValue == NO_DATA_VALUE) validValue = d;

            if (a == NO_DATA_VALUE) a = validValue;
            if (b == NO_DATA_VALUE) b = validValue;
            if (c == NO_DATA_VALUE) c = validValue;
            if (d == NO_DATA_VALUE) d = validValue;
        }

        return true;
    }
}

//Heightfield::Heightfield(shared_ptr<Image> image)
//    : _image(image)
//{
//    ROCKY_SOFT_ASSERT(image != nullptr);
//    ROCKY_SOFT_ASSERT(image->pixelFormat() == Image::R32_SFLOAT);
//}

Heightfield::Heightfield() :
    super(Image::R32_SFLOAT, 0, 0, 1)
{
    //todo
}

Heightfield::Heightfield(unsigned cols, unsigned rows) :
    super(Image::R32_SFLOAT, cols, rows, 1)
{
    //nop
}

Heightfield::Heightfield(Image* image)
{
    if (image)
    {
        _pixelFormat = image->pixelFormat();
        _width = image->width();
        _height = image->height();
        _depth = image->depth();
        _data = image->releaseData();
    }
}

const Heightfield*
Heightfield::cast_from(const Image* rhs)
{
    if (rhs && rhs->pixelFormat() == PixelFormat::R32_SFLOAT)
        return reinterpret_cast<const Heightfield*>(rhs);
    else
        return nullptr;
}

float
Heightfield::heightAtUV(
    double nx, double ny,
    Interpolation interp) const
{
    double px = clamp(nx, 0.0, 1.0) * (double)(width() - 1);
    double py = clamp(ny, 0.0, 1.0) * (double)(height() - 1);
    return heightAtPixel(px, py, interp);
}

float
Heightfield::heightAtPixel(
    double c,
    double r,
    Interpolation interpolation) const
{
    float result = 0.0;
    switch (interpolation)
    {
    case BILINEAR:
    {
        //OE_INFO << "getHeightAtPixel: (" << c << ", " << r << ")" << std::endl;
        int rowMin = std::max((int)floor(r), 0);
        int rowMax = std::max(std::min((int)ceil(r), (int)(height() - 1)), 0);
        int colMin = std::max((int)floor(c), 0);
        int colMax = std::max(std::min((int)ceil(c), (int)(width() - 1)), 0);

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = heightAt(colMax, rowMax);
        float llHeight = heightAt(colMin, rowMin);
        float ulHeight = heightAt(colMin, rowMax);
        float lrHeight = heightAt(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (!validateSamples(urHeight, llHeight, ulHeight, lrHeight))
        {
            return NO_DATA_VALUE;
        }

        //OE_INFO << "Heights (ll, lr, ul, ur) ( " << llHeight << ", " << urHeight << ", " << ulHeight << ", " << urHeight << std::endl;

        //Check for exact value
        if ((colMax == colMin) && (rowMax == rowMin))
        {
            //OE_NOTICE << "Exact value" << std::endl;
            result = heightAt((unsigned)c, (unsigned)r);
        }
        else if (colMax == colMin)
        {
            //OE_NOTICE << "Vertically" << std::endl;
            //Linear interpolate vertically
            result = ((double)rowMax - r) * llHeight + (r - (double)rowMin) * ulHeight;
        }
        else if (rowMax == rowMin)
        {
            //OE_NOTICE << "Horizontally" << std::endl;
            //Linear interpolate horizontally
            result = ((double)colMax - c) * llHeight + (c - (double)colMin) * lrHeight;
        }
        else
        {
            //OE_NOTICE << "Bilinear" << std::endl;
            //Bilinear interpolate
            double r1 = ((double)colMax - c) * (double)llHeight + (c - (double)colMin) * (double)lrHeight;
            double r2 = ((double)colMax - c) * (double)ulHeight + (c - (double)colMin) * (double)urHeight;
            result = ((double)rowMax - r) * (double)r1 + (r - (double)rowMin) * (double)r2;
        }
        break;
    }
    case AVERAGE:
    {
        //OE_INFO << "getHeightAtPixel: (" << c << ", " << r << ")" << std::endl;
        int rowMin = std::max((int)floor(r), 0);
        int rowMax = std::max(std::min((int)ceil(r), (int)(height() - 1)), 0);
        int colMin = std::max((int)floor(c), 0);
        int colMax = std::max(std::min((int)ceil(c), (int)(width() - 1)), 0);

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = heightAt(colMax, rowMax);
        float llHeight = heightAt(colMin, rowMin);
        float ulHeight = heightAt(colMin, rowMax);
        float lrHeight = heightAt(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (!validateSamples(urHeight, llHeight, ulHeight, lrHeight))
        {
            return NO_DATA_VALUE;
        }

        //OE_INFO << "Heights (ll, lr, ul, ur) ( " << llHeight << ", " << urHeight << ", " << ulHeight << ", " << urHeight << std::endl;

        double x_rem = c - (int)c;
        double y_rem = r - (int)r;

        double w00 = (1.0 - y_rem) * (1.0 - x_rem) * (double)llHeight;
        double w01 = (1.0 - y_rem) * x_rem * (double)lrHeight;
        double w10 = y_rem * (1.0 - x_rem) * (double)ulHeight;
        double w11 = y_rem * x_rem * (double)urHeight;

        result = (float)(w00 + w01 + w10 + w11);
        break;
    }
    case NEAREST:
    {
        //Nearest interpolation
        result = heightAt((unsigned)round(c), (unsigned)round(r));
        break;
    }
    case TRIANGULATE:
    {
        //Interpolation to make sure that the interpolated point follows the triangles generated by the 4 parent points
        int rowMin = std::max((int)floor(r), 0);
        int rowMax = std::max(std::min((int)ceil(r), (int)(height() - 1)), 0);
        int colMin = std::max((int)floor(c), 0);
        int colMax = std::max(std::min((int)ceil(c), (int)(width() - 1)), 0);

        if (rowMin == rowMax)
        {
            if (rowMin < (int)height() - 1)
            {
                rowMax = rowMin + 1;
            }
            else if (rowMax > 0)
            {
                rowMin = rowMax - 1;
            }
        }

        if (colMin == colMax)
        {
            if (colMin < (int)(width() - 1))
            {
                colMax = colMin + 1;
            }
            else if (colMax > 0)
            {
                colMin = colMax - 1;
            }
        }

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = heightAt(colMax, rowMax);
        float llHeight = heightAt(colMin, rowMin);
        float ulHeight = heightAt(colMin, rowMax);
        float lrHeight = heightAt(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (!validateSamples(urHeight, llHeight, ulHeight, lrHeight))
        {
            return NO_DATA_VALUE;
        }


        //The quad consisting of the 4 corner points can be made into two triangles.
        //The "left" triangle is ll, ur, ul
        //The "right" triangle is ll, lr, ur

        //Determine which triangle the point falls in.
        glm::dvec3 v0, v1, v2;

        double dx = c - (double)colMin;
        double dy = r - (double)rowMin;

        if (dx > dy)
        {
            //The point lies in the right triangle
            v0 = glm::dvec3(colMin, rowMin, llHeight);
            v1 = glm::dvec3(colMax, rowMin, lrHeight);
            v2 = glm::dvec3(colMax, rowMax, urHeight);
        }
        else
        {
            //The point lies in the left triangle
            v0 = glm::dvec3(colMin, rowMin, llHeight);
            v1 = glm::dvec3(colMax, rowMax, urHeight);
            v2 = glm::dvec3(colMin, rowMax, ulHeight);
        }

        //Compute the normal
        glm::dvec3 n = glm::cross((v1 - v0), (v2 - v0));

        result = (n.x * (c - v0.x) + n.y*(r - v0.y)) / -n.z + v0.z;
        //result = (n.x() * (c - v0.x()) + n.y() * (r - v0.y())) / -n.z() + v0.z();
        break;
    }
    }

    return result;
}

void
Heightfield::fill(float value)
{
    float* ptr = data<float>();
    for (unsigned i = 0; i < sizeInPixels(); ++i)
        *ptr++ = value;
}
