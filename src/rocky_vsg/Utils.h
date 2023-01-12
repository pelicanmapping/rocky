/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Image.h>
#include <rocky/Math.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/mat4.h>
#include <vsg/vk/State.h>

namespace ROCKY_NAMESPACE
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

    inline float distanceTo(const vsg::dvec3& p, vsg::State* state) {
        return vsg::length(state->modelviewMatrixStack.top() * p);
    }

    // adapted from vsg
    template<typename T>
    vsg::ref_ptr<vsg::Data> move(shared_ptr<Image> image, VkFormat format)
    {
        // NB!
        // We copy the values out of image FIRST because once we call
        // image->releaseData() they will all reset!
        unsigned
            width = image->width(),
            height = image->height(),
            depth = image->depth();

        T* data = reinterpret_cast<T*>(image->releaseData());

        vsg::ref_ptr<vsg::Data> vsg_data;
        if (depth == 1)
        {
            vsg_data = vsg::Array2D<T>::create(
                width, height,
                data,
                vsg::Data::Layout{ format });
        }
        else
        {
            vsg_data = vsg::Array3D<T>::create(
                width, height, depth,
                data,
                vsg::Data::Layout{ format });
        }

        //if (image->origin() == Image::BOTTOM_LEFT)
        //    vsg_data->properties.origin = vsg::TOP_LEFT;
        //else
        //    vsg_data->properties.origin = vsg::BOTTOM_LEFT;

        return vsg_data;
    }

    // adapted from vsg
    inline vsg::ref_ptr<vsg::Data> moveImageData(shared_ptr<Image> image)
    {
        if (!image) return { };

        switch (image->pixelFormat())
        {
        case Image::R8_UNORM:
            return move<unsigned char>(image, VK_FORMAT_R8_UNORM);
            break;
        case Image::R8G8_UNORM:
            return move<vsg::ubvec2>(image, VK_FORMAT_R8G8_UNORM);
            break;
        case Image::R8G8B8_UNORM:
            return move<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_UNORM);
            break;
        case Image::R8G8B8A8_UNORM:
            return move<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_UNORM);
            break;
        case Image::R16_UNORM:
            return move<unsigned short>(image, VK_FORMAT_R16_UNORM);
            break;
        case Image::R32_SFLOAT:
            return move<float>(image, VK_FORMAT_R32_SFLOAT);
            break;
        case Image::R64_SFLOAT:
            return move<double>(image, VK_FORMAT_R64_SFLOAT);
            break;
        };

        return { };
    }

    // adapted from vsg.
    // take ownership of the input image as a VSG object.
    // the input image becomes INVALID after this method. If that's not what
    // you want, clone the input image first!
    inline vsg::ref_ptr<vsg::Data> moveImageToVSG(shared_ptr<Image> image)
    {
        if (!image)
            return {};

        auto data = moveImageData(image);
        data->properties.origin = vsg::TOP_LEFT;
        data->properties.maxNumMipmaps = 1;

        return data;
    }

    // conver a vsg::Data structure to an Image if possible
    inline Result<shared_ptr<Image>> makeImageFromVSG(vsg::ref_ptr<vsg::Data> data)
    {
        if (!data)
            return Status(Status::ResourceUnavailable);

        // TODO: move this into a utility somewhere
        auto vkformat = data->properties.format;

        Image::PixelFormat format =
            vkformat == VK_FORMAT_R8_UNORM ? Image::R8_UNORM :
            vkformat == VK_FORMAT_R8G8_UNORM ? Image::R8G8_UNORM :
            vkformat == VK_FORMAT_R8G8B8_UNORM ? Image::R8G8B8_UNORM :
            vkformat == VK_FORMAT_R8G8B8A8_UNORM ? Image::R8G8B8A8_UNORM :
            vkformat == VK_FORMAT_R16_UNORM ? Image::R16_UNORM :
            vkformat == VK_FORMAT_R32_SFLOAT ? Image::R32_SFLOAT :
            vkformat == VK_FORMAT_R64_SFLOAT ? Image::R64_SFLOAT :
            Image::UNDEFINED;

        if (format == Image::UNDEFINED)
        {
            return Status(Status::ResourceUnavailable, "Unsupported image format");
        }

        auto image = Image::create(
            format,
            data->width(),
            data->height(),
            data->depth());

        memcpy(image->data<uint8_t>(), data->dataPointer(), image->sizeInBytes());

        if (data->properties.origin == vsg::TOP_LEFT)
        {
            image->flipVerticalInPlace();
        }

        return Result(image);
    }
}
