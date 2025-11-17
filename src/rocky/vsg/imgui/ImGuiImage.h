/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>

#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
#if IMGUI_VERSION_NUM >= 19200
    using ImGuiTextureHandle = ImTextureRef;
#else
    using ImGuiTextureHandle = ImTextureID;
#endif

    /**
    * ImGuiImage encapsulates a texture that you can use in ImGui::Image() with its
    * Vulkan backend.
    * - Create the ImGuiImage
    * - call ImGui::Image(im->id(deviceID), im->size())
    */
    class ROCKY_EXPORT ImGuiImage
    {
    public:
        //! Default constructor - invalid object
        ImGuiImage() = default;

        //! Construct a new widget texture from an Image::Ptr.
        ImGuiImage(Image::Ptr image, VSGContext context);

        //! Opaque image handle to pass to ImGui::Image() that uses
        //! the device ID this image was created with
        ImGuiTextureHandle handle() const;

        //! Native image size to pass to ImGui::Image()
        inline ImVec2 size() const {
            return ImVec2(_image->width(), _image->height());
        }

        //! Is this image valid?
        inline operator bool() const {
            return _internal;
        }

    protected:
        struct Internal;
        Image::Ptr _image;
        Internal* _internal = nullptr;
        std::uint32_t _deviceID = 0;
    };
};

#endif
