/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>

#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <imgui_internal.h>
#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
#if IMGUI_VERSION_NUM >= 19200
    using ImGuiTextureHandle = ImTextureRef;
#else
    using ImGuiTextureHandle = ImTextureID;
#endif

    /**
    * ImGuiImage encapsulates a Rocky Image so that you can render it was an ImGui::Image.
    * - Create the ImGuiImage from an existing rocky::Image
    * - call ImGui::Image(im.handle(), im.size());
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

        //! Opaque image im to padd to ImGui::AddImageQuad() if necessary
        ImTextureID id() const;

        //! Render this image at the specified size and with the specified rotation.
        inline void render(ImVec2 size, float rotationDegrees = 0.0f) const;

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
    };


    inline void ImGuiImage::render(ImVec2 size, float rotationDegrees) const
    {
        if (rotationDegrees == 0.0f)
        {
            ImGui::Image(handle(), size);
            return;
        }

        auto* window = ImGui::GetCurrentWindow();

        float cos_a = cosf(glm::radians(rotationDegrees));
        float sin_a = sinf(glm::radians(rotationDegrees));

        ImVec2 half_size = ImVec2(size.x * 0.5f, size.y * 0.5f);
        
        // 4 corners relative to center (unrotated)
        ImVec2 corners[4] = {
            ImVec2(-half_size.x, -half_size.y),
            ImVec2(half_size.x, -half_size.y),
            ImVec2(half_size.x,  half_size.y),
            ImVec2(-half_size.x,  half_size.y)
        };

        ImVec2 center(window->DC.CursorPos.x + half_size.x, window->DC.CursorPos.y + half_size.y);

        // Rotate + translate corners
        for (int i = 0; i < 4; ++i)
        {
            float x = corners[i].x * cos_a - corners[i].y * sin_a;
            float y = corners[i].x * sin_a + corners[i].y * cos_a;
            corners[i].x = center.x + x;
            corners[i].y = center.y + y;
        }

        const ImRect bb(
            ImVec2(
                std::min({ corners[0].x, corners[1].x, corners[2].x, corners[3].x }),
                std::min({ corners[0].y, corners[1].y, corners[2].y, corners[3].y })),
            ImVec2(
                std::max({ corners[0].x, corners[1].x, corners[2].x, corners[3].x }),
                std::max({ corners[0].y, corners[1].y, corners[2].y, corners[3].y })));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, 0))
            return;

        window->DrawList->AddImageQuad(id(), corners[0], corners[1], corners[2], corners[3]);
    }
};

#endif
