/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Math.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/mat4.h>

namespace rocky
{
    inline fvec3 to_glm(const vsg::vec3& a) {
        return fvec3(a.x, a.y, a.z);
    }
    inline dvec3 to_glm(const vsg::dvec3& a) {
        return dvec3(a.x, a.y, a.z);
    }
    inline fmat4 to_glm(const vsg::mat4& a) {
        return fmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline dmat4 to_glm(const vsg::dmat4& a) {
        return dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }

    inline vsg::vec3 to_vsg(const fvec3& a) {
        return vsg::vec3(a.x, a.y, a.z);
    }
    inline vsg::dvec3 to_vsg(const dvec3& a) {
        return vsg::dvec3(a.x, a.y, a.z);
    }
    inline vsg::mat4 to_vsg(const fmat4& a) {
        return vsg::mat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline vsg::dmat4 to_vsg(const dmat4& a) {
        return vsg::dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }

    // adapted from vsg
    template<typename T>
    vsg::ref_ptr<vsg::Data> create(shared_ptr<Image> image, VkFormat format)
    {
        vsg::ref_ptr<vsg::Data> vsg_data;
        if (image->depth() == 1)
        {
            vsg_data = vsg::Array2D<T>::create(
                image->width(), image->height(),
                image->data<T>(),
                vsg::Data::Layout{ format });
        }
        else
        {
            vsg_data = vsg::Array3D<T>::create(
                image->width(), image->height(), image->depth(),
                image->data<T>(),
                vsg::Data::Layout{ format });
        }

        return vsg_data;
    }

    // adapted from vsg
    inline vsg::ref_ptr<vsg::Data> createExpandedData(shared_ptr<Image> image)
    {
        if (image->componentSizeInBytes() != 1)
            return {};

        unsigned sourceSize = image->sizeInBytes(); // getTotalSizeInBytesIncludingMipmaps();
        unsigned sourceElements = sourceSize / 3;
        auto sourceData = image->data<unsigned char>();
        vsg::ubvec4* destData = new vsg::ubvec4[sourceElements];
        const unsigned char* srcPtr = sourceData;
        for (unsigned i = 0; i < sourceElements; ++i)
        {
            for (unsigned j = 0; j < 3; ++j)
                destData[i][j] = *srcPtr++;
            destData[i][3] = 255;
        }
        vsg::Data::Layout layout;
        layout.format = VK_FORMAT_R8G8B8A8_UNORM;

        //layout.maxNumMipmaps = image->getNumMipmapLevels();
        //if (image->getOrigin() == osg::Image::BOTTOM_LEFT)
        //    layout.origin = vsg::BOTTOM_LEFT;
        //else
        //    layout.origin = vsg::TOP_LEFT;

        return vsg::Array2D<vsg::ubvec4>::create(
            image->width(), image->height(),
            destData,
            vsg::Data::Layout{ VK_FORMAT_R8G8B8A8_UNORM });
    }

    // adapted from vsg
    inline vsg::ref_ptr<vsg::Data> convertImage(const shared_ptr<Image> image)
    {
        if (!image) return { };

        switch (image->pixelFormat())
        {
        case Image::R8_UNORM:
            return create<unsigned char>(image, VK_FORMAT_R8_UNORM);
            break;
        case Image::R8G8_UNORM:
            return create<vsg::ubvec2>(image, VK_FORMAT_R8G8_UNORM);
            break;
        case Image::R8G8B8_UNORM:
            return create<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_UNORM);
            break;
        case Image::R8G8B8A8_UNORM:
            return create<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_UNORM);
            break;
        case Image::R16_UNORM:
            return create<unsigned short>(image, VK_FORMAT_R16_UNORM);
            break;
        case Image::R32_SFLOAT:
            return create<float>(image, VK_FORMAT_R32_SFLOAT);
            break;
        case Image::R64_SFLOAT:
            return create<double>(image, VK_FORMAT_R64_SFLOAT);
            break;
        };

        return { };
    }

    // adapted from vsg
    inline vsg::ref_ptr<vsg::Data> convertImageToVSG(const shared_ptr<Image> image)
    {
        if (!image)
            return {};

        auto data = convertImage(image);
        data->properties.origin = vsg::TOP_LEFT;
        data->properties.maxNumMipmaps = 1;

        return data;
    }
}
