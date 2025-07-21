/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>

#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT WidgetImage : public vsg::Inherit<vsg::Compilable, WidgetImage>
    {
    public:
        //! Construct a new widget texture from an Image::Ptr.
        WidgetImage(Image::Ptr image);

        ImTextureID id(std::uint32_t deviceID) const;

        ImVec2 size() const {
            return ImVec2(_image->width(), _image->height());
        }

    public:
        //! VSG compile
        void compile(vsg::Context& context) override;

    protected:
        struct Internal;
        Image::Ptr _image;
        Internal* _internal = nullptr;
        bool _compiled = false;
    };
};

#endif
