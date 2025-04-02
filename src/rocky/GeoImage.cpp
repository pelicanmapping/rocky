/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoImage.h"
#include "Math.h"
#include "Image.h"

#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_proxy.h>
#include <cpl_string.h>
#include <gdal_vrt.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
#ifdef ROCKY_HAS_GDAL
    std::shared_ptr<Image> createImageFromDataset(GDALDataset* ds)
    {
        // called internally -- GDAL lock not required

        int numBands = ds->GetRasterCount();
        if (numBands < 1)
            return nullptr;

        //int dataType;
        int sampleSize;
        Image::PixelFormat pixelFormat;

        switch (ds->GetRasterBand(1)->GetRasterDataType())
        {
        case GDT_Byte:
            //dataType = GL_UNSIGNED_BYTE;
            sampleSize = 1;
            pixelFormat =
                numBands == 1 ? Image::R8_UNORM :
                numBands == 2 ? Image::R8G8_UNORM :
                numBands == 3 ? Image::R8G8B8_UNORM :
                Image::R8G8B8A8_UNORM;
            break;
        case GDT_UInt16:
        case GDT_Int16:
            //dataType = GL_UNSIGNED_SHORT;
            sampleSize = 2;
            //internalFormat = GL_R16;
            pixelFormat = Image::R16_UNORM;
            break;
        default:
            //dataType = GL_FLOAT;
            sampleSize = 4;
            pixelFormat = Image::R32_SFLOAT;
        }

        int pixelBytes = sampleSize * numBands;

        //Allocate the image
        auto image = Image::create(
            pixelFormat,
            (unsigned)ds->GetRasterXSize(),
            (unsigned)ds->GetRasterYSize());

        CPLErr err = ds->RasterIO(
            GF_Read,
            0, 0,
            image->width(), image->height(),
            image->data<void>(),
            image->width(), image->height(),
            ds->GetRasterBand(1)->GetRasterDataType(),
            numBands,
            NULL,
            pixelBytes,
            pixelBytes * image->width(),
            1);

        if (err != CE_None)
        {
            std::cerr << "[gdal] RasterIO failed.\n";
            return nullptr;
        }

        ds->FlushCache();

        image->flipVerticalInPlace();

        return image;
    }

    GDALDataset* createMemDS(int width, int height, int numBands, GDALDataType dataType, double minX, double minY, double maxX, double maxY, const std::string &projection)
    {
        //Get the MEM driver
        GDALDriver* memDriver = (GDALDriver*)GDALGetDriverByName("MEM");
        if (!memDriver)
        {
            //ROCKY_WARN << "[gdal] Could not get MEM driver" << std::endl;
            return NULL;
        }

        //Create the in memory dataset.
        GDALDataset* ds = memDriver->Create("", width, height, numBands, dataType, 0);
        if (!ds)
        {
            //ROCKY_WARN << "[gdal] memDriver.create failed" << std::endl;
            return NULL;
        }

        //Initialize the color interpretation
        if (numBands == 1)
        {
            ds->GetRasterBand(1)->SetColorInterpretation(GCI_GrayIndex);
        }
        else
        {
            if (numBands >= 1)
                ds->GetRasterBand(1)->SetColorInterpretation(GCI_RedBand);
            if (numBands >= 2)
                ds->GetRasterBand(2)->SetColorInterpretation(GCI_GreenBand);
            if (numBands >= 3)
                ds->GetRasterBand(3)->SetColorInterpretation(GCI_BlueBand);
            if (numBands >= 4)
                ds->GetRasterBand(4)->SetColorInterpretation(GCI_AlphaBand);
        }

        //Initialize the geotransform
        double geotransform[6];
        double x_units_per_pixel = (maxX - minX) / (double)width;
        double y_units_per_pixel = (maxY - minY) / (double)height;
        geotransform[0] = minX;
        geotransform[1] = x_units_per_pixel;
        geotransform[2] = 0;
        geotransform[3] = maxY;
        geotransform[4] = 0;
        geotransform[5] = -y_units_per_pixel;
        ds->SetGeoTransform(geotransform);
        ds->SetProjection(projection.c_str());

        return ds;
    }

    GDALDataset* createDataSetFromImage(const Image* image, double minX, double minY, double maxX, double maxY, const std::string &projection)
    {
        //Clone the incoming image
        std::shared_ptr<Image> clonedImage = image->clone();

        //Flip the image
        clonedImage->flipVerticalInPlace();

        GDALDataType gdalDataType =
            image->pixelFormat() <= Image::R8G8B8A8_UNORM ? GDT_Byte :
            image->pixelFormat() <= Image::R16_UNORM ? GDT_UInt16 :
            GDT_Float32;

            //image->pixelFormat <= 
            //image->getDataType() == GL_UNSIGNED_BYTE ? GDT_Byte :
            //image->getDataType() == GL_UNSIGNED_SHORT ? GDT_UInt16 :
            //image->getDataType() == GL_FLOAT ? GDT_Float32 :
            //GDT_Byte;

        int numBands = image->numComponents();

        if (numBands == 0)
        {
            //ROCKY_WARN << "[gdal] Failure in createDataSetFromImage: unsupported pixel format\n";
            return nullptr;
        }

        int pixelBytes =
            gdalDataType == GDT_Byte ? numBands :
            gdalDataType == GDT_UInt16 ? 2 * numBands :
            4 * numBands;

        GDALDataset* srcDS = createMemDS(
            image->width(), image->height(),
            numBands,
            gdalDataType,
            minX, minY, maxX, maxY,
            projection);

        if (srcDS)
        {
            CPLErr err = srcDS->RasterIO(
                GF_Write,
                0, 0,
                clonedImage->width(), clonedImage->height(),
                clonedImage->data<void>(),
                clonedImage->width(),
                clonedImage->height(),
                gdalDataType,
                numBands,
                NULL,
                pixelBytes,
                pixelBytes * image->width(),
                1);

            if (err != CE_None)
            {
                //ROCKY_WARN << "RasterIO failed.\n";
            }

            srcDS->FlushCache();
        }

        return srcDS;
    }

    std::shared_ptr<Image> GDAL_reprojectImage(
        const Image* srcImage,
        const std::string srcWKT,
        double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
        const std::string destWKT,
        double destMinX, double destMinY, double destMaxX, double destMaxY,
        int width,
        int height,
        bool useBilinearInterpolation)
    {
        //Create a dataset from the source image
        GDALDataset* srcDS = createDataSetFromImage(srcImage, srcMinX, srcMinY, srcMaxX, srcMaxY, srcWKT);

        if (srcDS == nullptr)
            return nullptr;

        if (width == 0 || height == 0)
        {
            double outgeotransform[6];
            double extents[4];
            void* transformer = GDALCreateGenImgProjTransformer(srcDS, srcWKT.c_str(), NULL, destWKT.c_str(), 1, 0, 0);
            GDALSuggestedWarpOutput2(srcDS,
                GDALGenImgProjTransform, transformer,
                outgeotransform,
                &width,
                &height,
                extents,
                0);
            GDALDestroyGenImgProjTransformer(transformer);
        }
        //OE_DEBUG << "Creating warped output of " << width << "x" << height << " in " << destWKT << std::endl;

        int numBands = srcDS->GetRasterCount();
        GDALDataType dataType = srcDS->GetRasterBand(1)->GetRasterDataType();

        GDALDataset* destDS = createMemDS(width, height, numBands, dataType, destMinX, destMinY, destMaxX, destMaxY, destWKT);

        if (useBilinearInterpolation == true)
        {
            GDALReprojectImage(srcDS, NULL,
                destDS, NULL,
                GRA_Bilinear,
                0, 0, 0, 0, 0);
        }
        else
        {
            GDALReprojectImage(srcDS, NULL,
                destDS, NULL,
                GRA_NearestNeighbour,
                0, 0, 0, 0, 0);
        }

        auto result = createImageFromDataset(destDS);

        delete srcDS;
        delete destDS;

        return result;
    }
#endif

    bool transformGrid(
        const SRS& fromSRS,
        const SRS& toSRS,
        double in_xmin, double in_ymin,
        double in_xmax, double in_ymax,
        double* x, double* y,
        unsigned int numx, unsigned int numy)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(fromSRS.valid() && toSRS.valid(), false);

        auto xform = fromSRS.to(toSRS);
        if (!xform.valid())
            return false;

        std::vector<glm::dvec3> points;

        const double dx = (in_xmax - in_xmin) / (numx - 1);
        const double dy = (in_ymax - in_ymin) / (numy - 1);

        unsigned int pixel = 0;
        double fc = 0.0;
        for (unsigned int c = 0; c < numx; ++c, ++fc)
        {
            const double dest_x = in_xmin + fc * dx;
            double fr = 0.0;
            for (unsigned int r = 0; r < numy; ++r, ++fr)
            {
                const double dest_y = in_ymin + fr * dy;
                points.emplace_back(dest_x, dest_y, 0);
                pixel++;
            }
        }

        if (xform.transformRange(points.begin(), points.end()))
        {
            for (unsigned i = 0; i < points.size(); ++i)
            {
                x[i] = points[i].x;
                y[i] = points[i].y;
            }
            return true;
        }
        return false;
    }

    std::shared_ptr<Image> manualReproject(
        const Image*      image,
        const GeoExtent&  src_extent,
        const GeoExtent&  dest_extent,
        bool              interpolate,
        unsigned int      width = 0,
        unsigned int      height = 0)
    {
        if (width == 0 || height == 0)
        {
            //If no width and height are specified, just use the minimum dimension for the image
            width = std::min(image->width(), image->height());
            height = std::min(image->width(), image->height());
        }

        std::shared_ptr<Image> result = Image::create(
            image->pixelFormat(),
            width, height, image->depth());

        //Initialize the image to be completely transparent/black
        memset(result->data<std::byte>(), 0, result->sizeInBytes());

        const double dx = dest_extent.width() / (double)width;
        const double dy = dest_extent.height() / (double)height;

        // offset the sample points by 1/2 a pixel so we are sampling "pixel center".
        // (This is especially useful in the UnifiedCubeProfile since it nullifes the chances for
        // edge ambiguity.)

        unsigned int numPixels = width * height;

        // Start by creating a sample grid over the destination
        // extent. These will be the source coordinates. Then, reproject
        // the sample grid into the source coordinate system.
        double *srcPointsX = new double[numPixels * 2];
        double *srcPointsY = srcPointsX + numPixels;

        transformGrid(
            dest_extent.srs(),
            src_extent.srs(),
            dest_extent.xmin() + .5 * dx, dest_extent.ymin() + .5 * dy,
            dest_extent.xmax() - .5 * dx, dest_extent.ymax() - .5 * dy,
            srcPointsX, srcPointsY, width, height);

        //ImageUtils::PixelReader ia(image);
        Image::Pixel color;
        Image::Pixel urColor;
        Image::Pixel llColor;
        Image::Pixel ulColor;
        Image::Pixel lrColor;

        double xfac = (image->width() - 1) / src_extent.width();
        double yfac = (image->height() - 1) / src_extent.height();

        for (auto depth = 0u; depth < image->depth(); depth++)
        {
            // Next, go through the source-SRS sample grid, read the color at each point from the source image,
            // and write it to the corresponding pixel in the destination image.
            int pixel = 0;
            double xfac = (image->width() - 1) / src_extent.width();
            double yfac = (image->height() - 1) / src_extent.height();
            for (unsigned int c = 0; c < width; ++c)
            {
                for (unsigned int r = 0; r < height; ++r)
                {
                    double src_x = srcPointsX[pixel];
                    double src_y = srcPointsY[pixel];

                    if (src_x < src_extent.xmin() || src_x > src_extent.xmax() || src_y < src_extent.ymin() || src_y > src_extent.ymax())
                    {
                        //If the sample point is outside of the bound of the source extent, increment the pixel and keep looping through.
                        //ROCKY_WARN << LC << "ERROR: sample point out of bounds: " << src_x << ", " << src_y << std::endl;
                        pixel++;
                        continue;
                    }

                    float px = (float)((src_x - src_extent.xmin()) * xfac);
                    float py = (float)((src_y - src_extent.ymin()) * yfac);

                    int px_i = clamp((int)round(px), 0, (int)image->width() - 1);
                    int py_i = clamp((int)round(py), 0, (int)image->height() - 1);

                    color = { 0,0,0,0 };

                    // TODO: consider this again later. Causes blockiness.
                    if (!interpolate) //! isSrcContiguous ) // non-contiguous space- use nearest neighbot
                    {
                        image->read(color, px_i, py_i, depth);
                        //ia(color, px_i, py_i, depth);
                    }

                    else // contiguous space - use bilinear sampling
                    {
                        int rowMin = std::max((int)floor(py), 0);
                        int rowMax = std::max(std::min((int)ceil(py), (int)(image->height() - 1)), 0);
                        int colMin = std::max((int)floor(px), 0);
                        int colMax = std::max(std::min((int)ceil(px), (int)(image->width() - 1)), 0);

                        if (rowMin > rowMax) rowMin = rowMax;
                        if (colMin > colMax) colMin = colMax;

                        image->read(urColor, colMax, rowMax, depth);
                        image->read(llColor, colMin, rowMin, depth);
                        image->read(ulColor, colMin, rowMax, depth);
                        image->read(lrColor, colMax, rowMin, depth);

                        /*Bilinear interpolation*/
                        //Check for exact value
                        if ((colMax == colMin) && (rowMax == rowMin))
                        {
                            //ROCKY_NOTICE << "[rocky::GeoData] Exact value" << std::endl;
                            image->read(color, px_i, py_i, depth);
                        }
                        else if (colMax == colMin)
                        {
                            //ROCKY_NOTICE << "[rocky::GeoData] Vertically" << std::endl;
                            //Linear interpolate vertically
                            for (unsigned int i = 0; i < 4; ++i)
                            {
                                color[i] = ((float)rowMax - py) * llColor[i] + (py - (float)rowMin) * ulColor[i];
                            }
                        }
                        else if (rowMax == rowMin)
                        {
                            //ROCKY_NOTICE << "[rocky::GeoData] Horizontally" << std::endl;
                            //Linear interpolate horizontally
                            for (unsigned int i = 0; i < 4; ++i)
                            {
                                color[i] = ((float)colMax - px) * llColor[i] + (px - (float)colMin) * lrColor[i];
                            }
                        }
                        else
                        {
                            //ROCKY_NOTICE << "[rocky::GeoData] Bilinear" << std::endl;
                            //Bilinear interpolate
                            float col1 = colMax - px, col2 = px - colMin;
                            float row1 = rowMax - py, row2 = py - rowMin;
                            for (unsigned int i = 0; i < 4; ++i)
                            {
                                float r1 = col1 * llColor[i] + col2 * lrColor[i];
                                float r2 = col1 * ulColor[i] + col2 * urColor[i];

                                //ROCKY_INFO << "r1, r2 = " << r1 << " , " << r2 << std::endl;
                                color[i] = row1 * r1 + row2 * r2;
                            }
                        }
                    }

                    result->write(color, c, r, depth);
                    pixel++;
                }
            }
        }

        delete[] srcPointsX;

        return result;
    }

    std::shared_ptr<Image>
    cropImage(
        const Image* image,
        double src_minx, double src_miny, double src_maxx, double src_maxy,
        double& dst_minx, double& dst_miny, double& dst_maxx, double& dst_maxy)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(image != nullptr, nullptr);

        //Compute the desired cropping rectangle
        unsigned windowX = clamp((unsigned)floor((dst_minx - src_minx) / (src_maxx - src_minx) * (double)image->width()), 0u, image->width() - 1);
        unsigned windowY = clamp((unsigned)floor((dst_miny - src_miny) / (src_maxy - src_miny) * (double)image->height()), 0u, image->height() - 1);
        unsigned windowWidth = clamp((unsigned)ceil((dst_maxx - src_minx) / (src_maxx - src_minx) * (double)image->width()) - windowX, 0u, image->width());
        unsigned windowHeight = clamp((unsigned)ceil((dst_maxy - src_miny) / (src_maxy - src_miny) * (double)image->height()) - windowY, 0u, image->height());

        if (windowX + windowWidth > image->width())
        {
            windowWidth = image->width() - windowX;
        }

        if (windowY + windowHeight > image->height())
        {
            windowHeight = image->height() - windowY;
        }

        if ((windowWidth * windowHeight) == 0)
        {
            return NULL;
        }

        //Compute the actual bounds of the area we are computing
        double res_s = (src_maxx - src_minx) / (double)image->width();
        double res_t = (src_maxy - src_miny) / (double)image->height();

        dst_minx = src_minx + (double)windowX * res_s;
        dst_miny = src_miny + (double)windowY * res_t;
        dst_maxx = dst_minx + (double)windowWidth * res_s;
        dst_maxy = dst_miny + (double)windowHeight * res_t;

        //OE_NOTICE << "Copying from " << windowX << ", " << windowY << ", " << windowWidth << ", " << windowHeight << std::endl;

        //Allocate the croppped image
        auto cropped = Image::create(
            image->pixelFormat(),
            windowWidth,
            windowHeight,
            image->depth());

        for (unsigned layer = 0; layer < image->depth(); ++layer)
        {
            for (unsigned src_row = windowY, dst_row = 0; dst_row < windowHeight; src_row++, dst_row++)
            {
                //if (src_row > image->t() - 1) OE_NOTICE << "HeightBroke" << std::endl;
                auto src_data = image->data<unsigned char>(windowX, src_row, layer);
                auto dst_data = cropped->data<unsigned char>(0, dst_row, layer);
                memcpy((void*)dst_data, (void*)src_data, cropped->rowSizeInBytes());
            }
        }
        return cropped;
    }
}



#define LC "[GeoImage] "

// static
GeoImage GeoImage::INVALID;


GeoImage::GeoImage() :
    _image(nullptr),
    _extent(GeoExtent::INVALID)
{
    //nop
}

GeoImage&
GeoImage::operator=(GeoImage&& rhs) noexcept
{
    if (this != &rhs)
    {
        _image = std::move(rhs._image);
        _extent = std::move(rhs._extent);
    }
    rhs._image = nullptr;
    rhs._extent = GeoExtent::INVALID;
    return *this;
}

GeoImage::GeoImage(std::shared_ptr<Image> image, const GeoExtent& extent) :
    _image(image),
    _extent(extent)
{
    //nop
}

bool
GeoImage::valid() const 
{
    return _extent.valid() && _image != nullptr;
}

std::shared_ptr<Image>
GeoImage::image() const
{
    return _image;
}

const SRS&
GeoImage::srs() const
{
    return _extent.srs();
}

const GeoExtent&
GeoImage::extent() const
{
    return _extent;
}

double
GeoImage::getUnitsPerPixel() const
{
    if (image())
    {
        double uppw = _extent.width() / (double)image()->width();
        double upph = _extent.height() / (double)image()->height();
        return (uppw + upph) / 2.0;
    }
    else return 0.0;
}

bool
GeoImage::getCoord(int s, int t, double& out_x, double& out_y) const
{
    if (!valid()) return false;
    if (!_image) return false;

    double u = (double)s / (double)(_image->width() - 1);
    double v = (double)t / (double)(_image->height() - 1);
    out_x = _extent.xmin() + u * _extent.width();
    out_y = _extent.ymin() + v * _extent.height();
    return true;
}

bool
GeoImage::getPixel(double x, double y, int& s, int& t) const
{
    if (!valid()) return false;
    if (!_image) return false;

    double u = (x - _extent.xmin()) / _extent.width();
    s = (u >= 0.0 && u <= 1.0) ? (int)(u * (double)(_image->width() - 1)) : -1;

    double v = (y - _extent.ymin()) / _extent.height();
    t = (v >= 0.0 && v <= 1.0) ? (int)(v * (double)(_image->height() - 1)) : -1;

    return true;
}

Result<GeoImage>
GeoImage::crop(
    const GeoExtent& e,
    bool exact, 
    unsigned int width, 
    unsigned int height, 
    bool useBilinearInterpolation) const
{
    if (!valid())
        return *this;

    if (!image())
        return Status(Status::ResourceUnavailable);

    // Check for equivalence
    if (e.srs().horizontallyEquivalentTo(srs()))
    {
        //If we want an exact crop or they want to specify the output size of the image, use GDAL
        if (exact || width != 0 || height != 0)
        {
            //Suggest an output image size
            if (width == 0 || height == 0)
            {
                double xRes = extent().width() / (double)image()->width();
                double yRes = extent().height() / (double)image()->height();

                width = std::max(1u, (unsigned int)(e.width() / xRes));
                height = std::max(1u, (unsigned int)(e.height() / yRes));
            }

            //Note:  Passing in the current SRS simply forces GDAL to not do any warping
            return reproject(srs(), &e, width, height, useBilinearInterpolation);
        }
        else
        {
            //If an exact crop is not desired, we can use the faster image cropping code that does no resampling.
            double destXMin = e.xmin();
            double destYMin = e.ymin();
            double destXMax = e.xmax();
            double destYMax = e.ymax();

            auto new_image = cropImage(
                image().get(),
                _extent.xmin(), _extent.ymin(), _extent.xmax(), _extent.ymax(),
                destXMin, destYMin, destXMax, destYMax);

            //The destination extents may be different than the input extents due to not being able to crop along pixel boundaries.
            return new_image ?
                GeoImage(new_image, GeoExtent(srs(), destXMin, destYMin, destXMax, destYMax)) :
                GeoImage::INVALID;
        }
    }
    else
    {
        return Status("Cropping extent does not have equivalent SpatialReference");
    }
}

Result<GeoImage>
GeoImage::reproject(
    const SRS& to_srs,
    const GeoExtent* to_extent,
    unsigned width,
    unsigned height,
    bool useBilinearInterpolation) const
{  
    GeoExtent destExtent;
    if (to_extent)
    {
        destExtent = *to_extent;
    }
    else
    {
        destExtent = extent().transform(to_srs);    
    }

    Image::ptr resultImage;


    bool reproject_with_gdal = false;

#ifndef ROCKY_HAS_GDAL
    reproject_with_gdal = image()->depth() == 1;
#endif


    //if (srs().isUserDefined() || to_srs.isUserDefined() || getImage()->depth() > 1)
    if (reproject_with_gdal == false)
    {
        // if either of the SRS is a custom projection or it is a 3D image, we have to do a manual reprojection since
        // GDAL will not recognize the SRS and does not handle 3D images.
        resultImage = manualReproject(
            image().get(),
            extent(),
            destExtent,
            useBilinearInterpolation,
            width,
            height);
    }

#ifdef ROCKY_HAS_GDAL
    else
    {
        // otherwise use GDAL.
        resultImage = GDAL_reprojectImage(
            image().get(),
            srs().wkt(),
            extent().xmin(), extent().ymin(), extent().xmax(), extent().ymax(),
            to_srs.wkt(),
            destExtent.xmin(), destExtent.ymin(), destExtent.xmax(), destExtent.ymax(),
            width,
            height,
            useBilinearInterpolation);
    }
#endif

    return GeoImage(resultImage, destExtent);
}

void
GeoImage::composite(const std::vector<GeoImage>& sources, const std::vector<float>& opacities)
{
    double x, y;
    glm::fvec4 pixel, temp;
    bool have_opacities = opacities.size() == sources.size();
    
    std::vector<SRSOperation> xforms;
    xforms.reserve(sources.size());
    for(auto& source : sources)
        xforms.emplace_back(srs().to(source.srs()));

        for (unsigned s = 0; s < _image->width(); ++s)
        {
            for (unsigned t = 0; t < _image->height(); ++t)
            {
                getCoord(s, t, x, y);

                for (unsigned layer = 0; layer < _image->depth(); ++layer)
                {
                    bool pixel_valid = false;

                    for (int i = 0; i < (int)sources.size(); ++i)
                    {
                        auto& source = sources[i];
                        float opacity = have_opacities ? opacities[i] : 1.0f;

                        if (!pixel_valid)
                        {
                            if (source.read(pixel, xforms[i], x, y, layer) && pixel.a > 0.0f)
                            {
                                pixel.a *= opacity;
                                pixel_valid = true;
                            }
                        }
                        else if (source.read(temp, xforms[i], x, y, layer))
                        {
                            pixel = glm::mix(pixel, temp, temp.a * opacity);
                        }
                    }

                    _image->write(pixel, s, t, layer);
                }
        }
    }
}

bool
GeoImage::read(glm::fvec4& output, const GeoPoint& p, int layer) const
{
    if (!p.valid() || !valid())
    {
        return false;
    }

    // transform if necessary
    if (!p.srs.horizontallyEquivalentTo(srs()))
    {
        GeoPoint c = p.transform(srs());
        return c.valid() && read(output, c);
    }

    double u = (p.x - _extent.xmin()) / _extent.width();
    double v = (p.y - _extent.ymin()) / _extent.height();

    // out of bounds?
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return false;
    }

    _image->read_bilinear(output, (float)u, (float)v, layer);

    return true;
}

bool
GeoImage::read(glm::fvec4& out, double x, double y, int layer) const
{
    if (!valid()) return false;

    double u = (x - _extent.xmin()) / _extent.width();
    double v = (y - _extent.ymin()) / _extent.height();

    // out of bounds?
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return false;
    }

    _image->read_bilinear(out, (float)u, (float)v, layer);
    return true;
}

bool
GeoImage::read_clamped(glm::fvec4& out, double x, double y, int layer) const
{
    if (!valid()) return false;

    double u = (x - _extent.xmin()) / _extent.width();
    double v = (y - _extent.ymin()) / _extent.height();

    _image->read_bilinear(out, (float)clamp(u, 0.0, 1.0), (float)clamp(v, 0.0, 1.0), layer);
    return true;
}

bool
GeoImage::read(glm::fvec4& out, const SRS& xy_srs, double x, double y, int layer) const
{
    if (!valid())
        return false;

    if (!xy_srs.valid())
        return false;

    glm::dvec3 temp(x, y, 0);
    if (!srs().to(xy_srs).transform(temp, temp))
        return false;

    return read(out, temp.x, temp.y, layer);
}

bool
GeoImage::read(glm::fvec4& out, const SRSOperation& xform, double x, double y, int layer) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), false);

    if (xform.noop())
        return read(out, x, y, layer);

    glm::dvec3 temp(x, y, 0);
    if (!xform.transform(temp, temp))
        return false;

    return read(out, temp.x, temp.y, layer);
}