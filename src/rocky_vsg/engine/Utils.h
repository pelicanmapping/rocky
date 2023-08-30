/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Image.h>
#include <rocky/Math.h>
#include <rocky/Threading.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/mat4.h>
#include <vsg/vk/State.h>
#include <vsg/vk/Context.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/threading/OperationThreads.h>

namespace ROCKY_NAMESPACE
{
    inline glm::fvec3 to_glm(const vsg::vec3& a) {
        return glm::fvec3(a.x, a.y, a.z);
    }
    inline glm::dvec3 to_glm(const vsg::dvec3& a) {
        return glm::dvec3(a.x, a.y, a.z);
    }
    inline glm::fmat4 to_glm(const vsg::mat4& a) {
        return glm::fmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline glm::dmat4 to_glm(const vsg::dmat4& a) {
        return glm::dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }

    inline vsg::vec3 to_vsg(const glm::fvec3& a) {
        return vsg::vec3(a.x, a.y, a.z);
    }
    inline vsg::dvec3 to_vsg(const glm::dvec3& a) {
        return vsg::dvec3(a.x, a.y, a.z);
    }
    inline vsg::mat4 to_vsg(const glm::fmat4& a) {
        return vsg::mat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline vsg::dmat4 to_vsg(const glm::dmat4& a) {
        return vsg::dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }

    inline float distanceTo(const vsg::dvec3& p, vsg::State* state) {
        return vsg::length(state->modelviewMatrixStack.top() * p);
    }

    namespace util
    {
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

            vsg::Data::Properties props;
            props.format = format;
            props.allocatorType = vsg::ALLOCATOR_TYPE_NEW_DELETE;

            vsg::ref_ptr<vsg::Data> vsg_data;
            if (depth == 1)
            {
                vsg_data = vsg::Array2D<T>::create(
                    width, height,
                    data,
                    props);
            }
            else
            {
                vsg_data = vsg::Array3D<T>::create(
                    width, height, depth,
                    data,
                    props);
            }

            //if (image->origin() == Image::BOTTOM_LEFT)
            //    vsg_data->properties.origin = vsg::TOP_LEFT;
            //else
            //    vsg_data->properties.origin = vsg::BOTTOM_LEFT;

            return vsg_data;
        }

        //! Moves a rocky Image object into a VSG Data object.
        //! The source Image is cleared in the process.
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

        // Convert a vsg::Data structure to an Image if possible
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

        /**
        * PromiseOperation combines a VSG operation with the Promise/Future construct
        * so that a VSG operation can return a future result.
        * 
        * Example: say you want to run something in VSG's update operations queue and
        * get the result when it's done:
        * 
        *   auto op = PromiseOperation<bool>::create();
        *   auto result = op->future();
        *   vsg_viewer->updateOperations->add(op);
        *   ... later, maybe during the next frame ...
        *   auto result = result.get();
        */
        template<class T>
        struct PromiseOperation : public vsg::Operation, public Cancelable
        {
            util::Future<T> _promise;
            std::function<T(Cancelable&)> _func;

            //! Was the operation canceled or abandoned?
            bool canceled() const override {
                return _promise.canceled();
            }

            //! Static factory function
            template<typename... Args>
            static vsg::ref_ptr<PromiseOperation> create(const Args&... args) {
                return vsg::ref_ptr<PromiseOperation>(new PromiseOperation(args...));
            }

            //! Construct a new promise operation with the function to execute
            //! @param func Function to execute
            PromiseOperation(std::function<T(Cancelable&)> func) :
                _func(func) { }

            //! Construct a new promise operation with the function to execute
            //! @param promise User-supplied promise object to use
            //! @param func Function to execute
            PromiseOperation(util::Future<T> promise, std::function<T(Cancelable&)> func) :
                _promise(promise),
                _func(func) { }

            //! Return the future result assocaited with this operation
            util::Future<T> future() {
                return _promise;
            }

            //! Runs the operation (don't call this directly)
            void run() override {
                if (!_promise.canceled())
                    _promise.resolve(_func(_promise));
                else
                    _promise.resolve();
            }
        };

#if 0
        vsg::ref_ptr<vsg::Image> createBasicImage(std::uint32_t width, std::uint32_t height, VkFormat format)
        {
            auto image = vsg::Image::create();
            image->imageType = VK_IMAGE_TYPE_2D;
            image->format = format; // VK_FORMAT_R8G8B8A8_UNORM;
            image->extent = VkExtent3D{ width, height, 1 };
            image->mipLevels = 1;
            image->arrayLayers = 1;
            image->samples = VK_SAMPLE_COUNT_1_BIT;
            image->tiling = VK_IMAGE_TILING_OPTIMAL;
            image->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image->flags = 0;
            image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            return image;
        }

        vsg::ref_ptr<vsg::Sampler> createBasicSampler()
        {
            auto sampler = vsg::Sampler::create();
            sampler->flags = 0;
            sampler->magFilter = VK_FILTER_LINEAR;
            sampler->minFilter = VK_FILTER_LINEAR;
            sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->mipLodBias = 0.0f;
            sampler->maxAnisotropy = 1.0f;
            sampler->minLod = 0.0f;
            sampler->maxLod = 1.0f;
            sampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            return sampler;
        }
#endif

        /**
        * Like vsg::CompileTraversal, but only for simple nodes and commands.
        */
        class SimpleCompiler : public vsg::Inherit<vsg::Visitor, SimpleCompiler>
        {
        public:
            vsg::Context& context;
            SimpleCompiler(vsg::Context& context_) : context(context_) { }

            void apply(vsg::Compilable& node) override {
                node.compile(context);
                node.traverse(*this);
            }
            void apply(vsg::Commands& commands) override {
                commands.compile(context);
                commands.traverse(*this);
            }
            void apply(vsg::StateGroup& stateGroup) override {
                stateGroup.compile(context);
                stateGroup.traverse(*this);
            }
            void apply(vsg::Geometry& geometry) {
                geometry.compile(context);
                geometry.traverse(*this);
            }
        };
    }
}
