/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Texture.h>
#include <vsg/state/ImageInfo.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Shareable VSG image and sampler for geometry and projected content.
     * Use only one texture source component on an entity. The supplied image
     * is retained by reference; call dirty() after changing its contents in place.
     * Replace imageInfo when changing image or sampler bindings.
     */
    struct Texture : public Component<Texture>
    {
        vsg::ref_ptr<vsg::ImageInfo> imageInfo;
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };

    //! Compatibility name for the shared VSG texture component.
    using MeshTexture = Texture;
}
