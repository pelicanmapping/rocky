/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/Threading.h>
#include <rocky/Image.h>
#include <rocky/Math.h>
#include <rocky/Status.h>
#include <rocky/weejobs.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/mat4.h>
#include <vsg/vk/State.h>
#include <vsg/vk/Context.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/threading/OperationThreads.h>
#include <queue>
#include <mutex>
#include <optional>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        // Visitor that finds the first node of a given type in a scene graph.
        template<class T>
        class FindNodeVisitor : public vsg::Inherit<vsg::Visitor, FindNodeVisitor<T>>
        {
        public:
            T* found = nullptr;
            void apply(vsg::Node& node) override
            {
                if (!found)
                {
                    found = node.cast<T>();
                    node.traverse(*this);
                }
            }
        };
    }

    // vsg-glm conversions.
    // These work because GLM and VSG classes are binary compatible.
    // C++ does not permit conversion operator overload outside of a class.

    inline const glm::fvec3& to_glm(const vsg::vec3& a) {
        return reinterpret_cast<const glm::fvec3&>(a);
    }
    inline const glm::dvec3& to_glm(const vsg::dvec3& a) {
        return reinterpret_cast<const glm::dvec3&>(a);
    }
    inline const glm::fmat4& to_glm(const vsg::mat4& a) {
        return reinterpret_cast<const glm::fmat4&>(a);
    }
    inline const glm::dmat4& to_glm(const vsg::dmat4& a) {
        return reinterpret_cast<const glm::dmat4&>(a);
    }

    inline const vsg::vec3& to_vsg(const glm::fvec3& a) {
        return reinterpret_cast<const vsg::vec3&>(a);
    }
    inline const vsg::dvec3& to_vsg(const glm::dvec3& a) {
        return reinterpret_cast<const vsg::dvec3&>(a);
    }
    inline const vsg::mat4& to_vsg(const glm::fmat4& a) {
        return reinterpret_cast<const vsg::mat4&>(a);
    }
    inline const vsg::dmat4& to_vsg(const glm::dmat4& a) {
        return reinterpret_cast<const vsg::dmat4&>(a);
    }

    //! Distance in scene units (meters) from a point to the camera.
    inline float distanceTo(const vsg::dvec3& p, vsg::State* state)
    {
        return (float)vsg::length(state->modelviewMatrixStack.top() * p);
    }

    //! Expands an existing sphere to include a point.
    template<typename vec_type>
    inline void expandBy(vsg::dsphere& bs, const vec_type& v)
    {        
        if (bs.valid())
        {
            auto dv = vsg::dvec3(v) - bs.center;
            auto r = vsg::length(dv);
            if (r > bs.radius)
            {
                auto dr = (r - bs.radius) * 0.5;
                bs.center += dv * (dr / r);
                bs.radius += dr;
            }
        }
        else
        {
            bs.center = v;
            bs.radius = 0.0;
        }
    }

    namespace util
    {
        //! Returns a vsg::Data structure pointing the the data in an image object
        //! without taking ownership of the data.
        template<typename T>
        vsg::ref_ptr<vsg::Data> wrap(std::shared_ptr<Image> image, VkFormat format)
        {
            unsigned
                width = image->width(),
                height = image->height(),
                depth = image->depth();

            T* data = image->data<T>(); // reinterpret_cast<T*>->data());

            vsg::Data::Properties props;
            props.format = format;
            props.allocatorType = vsg::ALLOCATOR_TYPE_NO_DELETE;

            vsg::ref_ptr<vsg::Data> vsg_data;
            if (depth == 1)
            {
                vsg_data = vsg::Array2D<T>::create(width, height, data, props);
            }
            else
            {
                vsg_data = vsg::Array3D<T>::create(width, height, depth, data, props);
            }

            //if (image->origin() == Image::BOTTOM_LEFT)
            //    vsg_data->properties.origin = vsg::TOP_LEFT;
            //else
            //    vsg_data->properties.origin = vsg::BOTTOM_LEFT;

            return vsg_data;
        }

        //! Wraps a rocky Image object in a VSG Data object.
        //! The source Image is not cleared in the process and data is now shared between the two.
        inline vsg::ref_ptr<vsg::Data> wrapImageData(std::shared_ptr<Image> image)
        {
            if (!image) return { };

            switch (image->pixelFormat())
            {
            case Image::R8_UNORM:
                return wrap<unsigned char>(image, VK_FORMAT_R8_UNORM);
                break;
            case Image::R8_SRGB:
                return wrap<unsigned char>(image, VK_FORMAT_R8_SRGB);
                break;
            case Image::R8G8_UNORM:
                return wrap<vsg::ubvec2>(image, VK_FORMAT_R8G8_UNORM);
                break;
            case Image::R8G8_SRGB:
                return wrap<vsg::ubvec2>(image, VK_FORMAT_R8G8_SRGB);
                break;
            case Image::R8G8B8_UNORM:
                return wrap<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_UNORM);
                break;
            case Image::R8G8B8_SRGB:
                return wrap<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_SRGB);
                break;
            case Image::R8G8B8A8_UNORM:
                return wrap<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_UNORM);
                break;
            case Image::R8G8B8A8_SRGB:
                return wrap<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_SRGB);
                break;
            case Image::R16_UNORM:
                return wrap<unsigned short>(image, VK_FORMAT_R16_UNORM);
                break;
            case Image::R32_SFLOAT:
                return wrap<float>(image, VK_FORMAT_R32_SFLOAT);
                break;
            case Image::R64_SFLOAT:
                return wrap<double>(image, VK_FORMAT_R64_SFLOAT);
                break;
            };

            return { };
        }

        //! Wraps a rocky Image object in a VSG Data object. Data is shared.
        inline vsg::ref_ptr<vsg::Data> wrapImageInVSG(std::shared_ptr<Image> image)
        {
            if (!image)
                return {};

            auto data = wrapImageData(image);
            data->properties.origin = vsg::TOP_LEFT;
            data->properties.maxNumMipmaps = 1;

            return data;
        }

        //! Returns a vsg::Data structure containing the data in an image, taking
        //! ownership of the data and reseting the image.
        template<typename T>
        vsg::ref_ptr<vsg::Data> move(std::shared_ptr<Image> image, VkFormat format)
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
                vsg_data = vsg::Array2D<T>::create(width, height, data, props);
            }
            else
            {
                vsg_data = vsg::Array3D<T>::create(width, height, depth, data, props);
            }

            //if (image->origin() == Image::BOTTOM_LEFT)
            //    vsg_data->properties.origin = vsg::TOP_LEFT;
            //else
            //    vsg_data->properties.origin = vsg::BOTTOM_LEFT;

            return vsg_data;
        }

        //! Moves a rocky Image object into a VSG Data object.
        //! The source Image is cleared in the process.
        inline vsg::ref_ptr<vsg::Data> moveImageData(std::shared_ptr<Image> image)
        {
            if (!image) return { };

            switch (image->pixelFormat())
            {
            case Image::R8_UNORM:
                return move<unsigned char>(image, VK_FORMAT_R8_UNORM);
                break;
            case Image::R8_SRGB:
                return move<unsigned char>(image, VK_FORMAT_R8_SRGB);
                break;
            case Image::R8G8_UNORM:
                return move<vsg::ubvec2>(image, VK_FORMAT_R8G8_UNORM);
                break;
            case Image::R8G8_SRGB:
                return move<vsg::ubvec2>(image, VK_FORMAT_R8G8_SRGB);
                break;
            case Image::R8G8B8_UNORM:
                return move<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_UNORM);
                break;
            case Image::R8G8B8_SRGB:
                return move<vsg::ubvec3>(image, VK_FORMAT_R8G8B8_SRGB);
                break;
            case Image::R8G8B8A8_UNORM:
                return move<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_UNORM);
                break;
            case Image::R8G8B8A8_SRGB:
                return move<vsg::ubvec4>(image, VK_FORMAT_R8G8B8A8_SRGB);
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
        inline vsg::ref_ptr<vsg::Data> moveImageToVSG(std::shared_ptr<Image> image)
        {
            if (!image)
                return {};

            auto data = moveImageData(image);
            data->properties.origin = vsg::TOP_LEFT;
            data->properties.maxNumMipmaps = 1;

            return data;
        }

        // Convert a vsg::Data structure to an Image if possible
        inline Result<std::shared_ptr<Image>> makeImageFromVSG(vsg::ref_ptr<vsg::Data> data)
        {
            if (!data)
                return Status(Status::ResourceUnavailable, "Data is empty");

            // TODO: move this into a utility somewhere
            auto vkformat = data->properties.format;

            Image::PixelFormat format =
                vkformat == VK_FORMAT_R8_UNORM        ? Image::R8_UNORM :
                vkformat == VK_FORMAT_R8_SRGB         ? Image::R8_SRGB :
                vkformat == VK_FORMAT_R8G8_UNORM      ? Image::R8G8_UNORM :
                vkformat == VK_FORMAT_R8G8_SRGB       ? Image::R8G8_SRGB :
                vkformat == VK_FORMAT_R8G8B8_UNORM    ? Image::R8G8B8_UNORM :
                vkformat == VK_FORMAT_R8G8B8_SRGB     ? Image::R8G8B8_SRGB :
                vkformat == VK_FORMAT_R8G8B8A8_UNORM  ? Image::R8G8B8A8_UNORM :
                vkformat == VK_FORMAT_R8G8B8A8_SRGB   ? Image::R8G8B8A8_SRGB :
                vkformat == VK_FORMAT_R16_UNORM       ? Image::R16_UNORM :
                vkformat == VK_FORMAT_R32_SFLOAT      ? Image::R32_SFLOAT :
                vkformat == VK_FORMAT_R64_SFLOAT      ? Image::R64_SFLOAT :
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
            jobs::promise<T> _promise;
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
            PromiseOperation(jobs::future<T> promise, std::function<T(Cancelable&)> func) :
                _promise(promise),
                _func(func) { }

            //! Return the future result assocaited with this operation
            jobs::future<T> future() {
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

        /**
        * vsg::Operation that executes a lambda function.
        */
        class LambdaOperation : public vsg::Inherit<vsg::Operation, LambdaOperation>
        {
        public:
            LambdaOperation(std::function<void()> func) : _func(func) { }
            void run() override { _func(); }
        private:
            std::function<void()> _func;
        };

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
                //stateGroup.compile(context);
                stateGroup.traverse(*this);
            }
            void apply(vsg::Geometry& geometry) {
                geometry.compile(context);
                geometry.traverse(*this);
            }
        };

        //! Finds the first node of a given type in a scene graph.
        template<class T>
        inline T* find(const vsg::ref_ptr<vsg::Node>& root)
        {
            detail::FindNodeVisitor<T> visitor;
            root->accept(visitor);
            return visitor.found;
        }
    }
}
