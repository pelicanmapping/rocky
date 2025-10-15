/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/Threading.h>
#include <rocky/Image.h>
#include <rocky/Math.h>
#include <rocky/Result.h>
#include <rocky/weejobs.h>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        // Visitor that finds the first node of a given type in a scene graph.
        template<class T>
        struct FindNodeVisitor : public vsg::Inherit<vsg::Visitor, FindNodeVisitor<T>>
        {
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

        template<class T>
        struct ForEachNodeVisitor : public vsg::Inherit<vsg::Visitor, ForEachNodeVisitor<T>>
        {
            std::function<void(T*)> _func;
            ForEachNodeVisitor(std::function<void(T*)> func)
                : _func(func) { }

            void apply(vsg::Object& object) override {
                if (auto t = object.cast<T>())
                    _func(t);
                object.traverse(*this);
            }
        };
    }

    // vsg-glm conversions.
    // These work because GLM and VSG classes are binary compatible.
    // C++ does not permit conversion operator overload outside of a class.

    inline const glm::fvec2& to_glm(const vsg::vec2& a) {
        return reinterpret_cast<const glm::fvec2&>(a);
    }
    inline const glm::dvec2& to_glm(const vsg::dvec2& a) {
        return reinterpret_cast<const glm::dvec2&>(a);
    }
    inline const glm::fvec3& to_glm(const vsg::vec3& a) {
        return reinterpret_cast<const glm::fvec3&>(a);
    }
    inline const glm::dvec3& to_glm(const vsg::dvec3& a) {
        return reinterpret_cast<const glm::dvec3&>(a);
    }
    inline const glm::fvec4& to_glm(const vsg::vec4& a) {
        return reinterpret_cast<const glm::fvec4&>(a);
    }
    inline const glm::dvec4& to_glm(const vsg::dvec4& a) {
        return reinterpret_cast<const glm::dvec4&>(a);
    }
    inline const glm::fmat4& to_glm(const vsg::mat4& a) {
        return reinterpret_cast<const glm::fmat4&>(a);
    }
    inline const glm::dmat4& to_glm(const vsg::dmat4& a) {
        return reinterpret_cast<const glm::dmat4&>(a);
    }
    inline const glm::fquat& to_glm(const vsg::quat& a) {
        return reinterpret_cast<const glm::fquat&>(a);
    }
    inline const glm::dquat& to_glm(const vsg::dquat& a) {
        return reinterpret_cast<const glm::dquat&>(a);
    }

    inline const vsg::vec2& to_vsg(const glm::fvec2& a) {
        return reinterpret_cast<const vsg::vec2&>(a);
    }
    inline const vsg::dvec2& to_vsg(const glm::dvec2& a) {
        return reinterpret_cast<const vsg::dvec2&>(a);
    }
    inline const vsg::vec3& to_vsg(const glm::fvec3& a) {
        return reinterpret_cast<const vsg::vec3&>(a);
    }
    inline const vsg::dvec3& to_vsg(const glm::dvec3& a) {
        return reinterpret_cast<const vsg::dvec3&>(a);
    }
    inline const vsg::vec4& to_vsg(const glm::fvec4& a) {
        return reinterpret_cast<const vsg::vec4&>(a);
    }
    inline const vsg::dvec4& to_vsg(const glm::dvec4& a) {
        return reinterpret_cast<const vsg::dvec4&>(a);
    }
    inline const vsg::mat4& to_vsg(const glm::fmat4& a) {
        return reinterpret_cast<const vsg::mat4&>(a);
    }
    inline const vsg::dmat4& to_vsg(const glm::dmat4& a) {
        return reinterpret_cast<const vsg::dmat4&>(a);
    }
    inline const vsg::quat& to_vsg(const glm::fquat& a) {
        return reinterpret_cast<const vsg::quat&>(a);
    }
    inline const vsg::dquat& to_vsg(const glm::dquat& a) {
        return reinterpret_cast<const vsg::dquat&>(a);
    }

    inline vsg::dbox to_vsg(const Box& box) {
        return vsg::dbox(
            vsg::dvec3(box.xmin, box.ymin, box.zmin),
            vsg::dvec3(box.xmax, box.ymax, box.zmax));
    }

    inline vsg::dsphere to_vsg(const Sphere& sphere) {
        return vsg::dsphere(
            vsg::dvec3(sphere.center.x, sphere.center.y, sphere.center.z),
            sphere.radius);
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

        inline Image::PixelFormat fromVkPixelFormat(VkFormat vkformat)
        {
            switch (vkformat)
            {
            case VK_FORMAT_R8_UNORM: return Image::R8_UNORM;
            case VK_FORMAT_R8_SRGB: return Image::R8_SRGB;
            case VK_FORMAT_R8G8_UNORM: return Image::R8G8_UNORM;
            case VK_FORMAT_R8G8_SRGB: return Image::R8G8_SRGB;
            case VK_FORMAT_R8G8B8_UNORM: return Image::R8G8B8_UNORM;
            case VK_FORMAT_R8G8B8_SRGB: return Image::R8G8B8_SRGB;
            case VK_FORMAT_R8G8B8A8_UNORM: return Image::R8G8B8A8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB: return Image::R8G8B8A8_SRGB;
            case VK_FORMAT_R16_UNORM: return Image::R16_UNORM;
            case VK_FORMAT_R32_SFLOAT: return Image::R32_SFLOAT;
            case VK_FORMAT_R64_SFLOAT: return Image::R64_SFLOAT;
            default: return Image::UNDEFINED;
            }
        }

        inline VkFormat toVkPixelFormat(Image::PixelFormat format)
        {
            switch (format)
            {
            case Image::R8_UNORM: return VK_FORMAT_R8_UNORM;
            case Image::R8_SRGB: return VK_FORMAT_R8_SRGB;
            case Image::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
            case Image::R8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
            case Image::R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
            case Image::R8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
            case Image::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
            case Image::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case Image::R16_UNORM: return VK_FORMAT_R16_UNORM;
            case Image::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
            case Image::R64_SFLOAT: return VK_FORMAT_R64_SFLOAT;
            default: return VK_FORMAT_UNDEFINED;
            }
        }


        // Convert a vsg::Data structure to an Image if possible
        inline Result<std::shared_ptr<Image>> makeImageFromVSG(vsg::ref_ptr<vsg::Data> data)
        {
            if (!data)
                return Failure(Failure::ResourceUnavailable, "Data is empty");

            // TODO: move this into a utility somewhere
            auto vkformat = data->properties.format;

            Image::PixelFormat format = fromVkPixelFormat(vkformat);

            if (format == Image::UNDEFINED)
            {
                return Failure(Failure::ResourceUnavailable, "Unsupported image format");
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

            return image;
        }

        inline vsg::ref_ptr<vsg::DescriptorImage> createTexture(Image::Ptr image,
            vsg::ref_ptr<vsg::Device> vsg_device)
        {
            //auto vsg_context = vsg::Context::create(vsg_device);

            //auto imageInfo = vsg::ImageInfo::create();

            auto colorImage = vsg::Image::create(moveImageData(image));
            colorImage->imageType = VK_IMAGE_TYPE_2D;
            colorImage->format = toVkPixelFormat(image->pixelFormat());
            colorImage->mipLevels = 1;
            colorImage->arrayLayers = 1;
            colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
            colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
            colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorImage->flags = 0;
            colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            //imageInfo->imageView = vsg::createImageView(*vsg_context, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);

            auto sampler = vsg::Sampler::create();
            sampler->magFilter = VK_FILTER_LINEAR;
            sampler->minFilter = VK_FILTER_LINEAR;
            sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->anisotropyEnable = VK_FALSE; // don't need this for a billboarded icon
            sampler->maxAnisotropy = 1.0f;
            sampler->maxLod = 1.0f;

            //imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            return vsg::DescriptorImage::create(
                sampler, moveImageData(image), 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
        inline T* find(vsg::Object* root)
        {
            detail::FindNodeVisitor<T> visitor;
            root->accept(visitor);
            return visitor.found;
        }

        //! Finds every node of a fiven type and runs a function against it
        template<class T>
        inline void forEach(vsg::Object* root, std::function<void(T*)> func)
        {
            if (!root) return;
            detail::ForEachNodeVisitor<T> visitor(func);
            root->accept(visitor);
        }

        //! Remove a node from a collection of nodes
        template<class COLLECTION>
        inline void remove(vsg::ref_ptr<vsg::Node> node, COLLECTION& collection)
        {
            if (!node) return;
            collection.erase(std::remove_if(collection.begin(), collection.end(),
                [&node](const vsg::ref_ptr<vsg::Node>& n) { return n == node; }),
                collection.end());
        }
    }
}
