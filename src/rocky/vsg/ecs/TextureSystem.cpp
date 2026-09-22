/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TextureSystem.h"
#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace ROCKY_NAMESPACE::detail
{
    //! Producer-owned cache of the ImageTexture generation and sampling metadata.
    struct ImageTextureDetail
    {
        Image::Ptr sourceImage;
        std::uint64_t sourceRevision = 0u;
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
        bool initialized = false;
        bool conflictLogged = false;
    };
}

TextureSystemNode::TextureSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
    {
        r.on_construct<ImageTexture>().connect<&TextureSystemNode::on_construct_ImageTexture>(*this);
        r.on_destroy<ImageTexture>().connect<&TextureSystemNode::on_destroy_ImageTexture>(*this);
        r.on_destroy<TextureResource>().connect<&TextureSystemNode::on_destroy_TextureResource>(*this);

        auto dirtyEntity = r.create();
        r.emplace<ImageTexture::Dirty>(dirtyEntity);
        r.emplace<TextureResource::Dirty>(dirtyEntity);

        r.view<ImageTexture>().each([&](auto entity, auto& imageTexture)
            {
                imageTexture.owner = entity;
                (void)r.get_or_emplace<ImageTextureDetail>(entity);
            });
    });
}

void TextureSystemNode::on_construct_ImageTexture(entt::registry& r, entt::entity entity)
{
    r.get<ImageTexture>(entity).owner = entity;
    (void)r.get_or_emplace<ImageTextureDetail>(entity);
}

void TextureSystemNode::on_destroy_ImageTexture(entt::registry& r, entt::entity entity)
{
    if (auto* detail = r.try_get<ImageTextureDetail>(entity))
    {
        if (auto* resource = r.try_get<TextureResource>(entity);
            resource && resource->producer == TextureResourceProducer::ImageTexture)
            r.remove<TextureResource>(entity);
        r.remove<ImageTextureDetail>(entity);
    }
}

void TextureSystemNode::on_destroy_TextureResource(entt::registry& r, entt::entity entity)
{
    // Hook the resource itself, not only ImageTexture: whole-entity destruction
    // may remove these components in either order. Other producers manage their
    // own resource lifetimes and must not be retired by this system.
    const auto& resource = r.get<TextureResource>(entity);
    if (resource.producer == TextureResourceProducer::ImageTexture && resource.texture)
    {
        std::scoped_lock lock(_pendingDisposalsMutex);
        _pendingDisposals.emplace_back(resource.texture);
    }
}

void TextureSystemNode::update(VSGContext vsgcontext)
{
    if (!vsgcontext)
        return;

    // DecalSystem can now relinquish the descriptor reference without freeing
    // an image still used by an in-flight frame. Drain even after a failure.
    {
        std::vector<vsg::ref_ptr<vsg::ImageInfo>> pending;
        {
            std::scoped_lock lock(_pendingDisposalsMutex);
            pending.swap(_pendingDisposals);
        }
        for (auto& image : pending)
            dispose(image);
    }

    if (status.failed())
    {
        Inherit::update(vsgcontext);
        return;
    }

    _registry.write([&](entt::registry& r)
    {
        r.view<ImageTexture, ImageTextureDetail>().each(
            [&](auto entity, auto& imageTexture, auto& detail)
            {
                auto* resource = r.try_get<TextureResource>(entity);
                if (
                    detail.initialized &&
                    resource &&
                    detail.sourceImage == imageTexture.image &&
                    detail.sourceRevision == imageTexture.componentRevision() &&
                    detail.origin == imageTexture.origin &&
                    detail.alphaMode == imageTexture.alphaMode)
                    return;

                if (resource && resource->producer != TextureResourceProducer::ImageTexture)
                {
                    if (!detail.conflictLogged)
                    {
                        Log()->warn("TextureSystemNode: entity {} already has a TextureResource owned by another producer", entt::to_integral(entity));
                        detail.conflictLogged = true;
                    }
                    return;
                }

                if (!resource)
                {
                    if (r.any_of<RenderTexture>(entity))
                    {
                        if (!detail.conflictLogged)
                        {
                            Log()->warn("TextureSystemNode: entity {} cannot contain both ImageTexture and RenderTexture producers", entt::to_integral(entity));
                            detail.conflictLogged = true;
                        }
                        return;
                    }
                    resource = &r.emplace<TextureResource>(entity);
                    resource->producer = TextureResourceProducer::ImageTexture;
                    resource->ready = false;
                }

                detail.conflictLogged = false;
                if (resource->texture)
                    dispose(resource->texture);

                resource->owner = entity;
                resource->texture = {};
                resource->ready = false;
                resource->origin = imageTexture.origin;
                resource->alphaMode = imageTexture.alphaMode;
                ++resource->revision;

                if (imageTexture.image)
                {
                    auto image = wrapImageInVSG(imageTexture.image);
                    if (image)
                    {
                        image->properties.dataVariance = vsg::DYNAMIC_DATA;
                        resource->texture = vsg::ImageInfo::create(vsg::Sampler::create(), image);
                        resource->ready = true;
                    }
                    else
                    {
                        Log()->warn("TextureSystemNode: failed to publish an ImageTexture");
                    }
                }
                else
                {
                    // A null image is a valid color-only/procedural projection.
                    // Publish readiness without allocating a descriptor texture.
                    resource->ready = true;
                }

                detail.sourceImage = imageTexture.image;
                detail.sourceRevision = imageTexture.componentRevision();
                detail.origin = imageTexture.origin;
                detail.alphaMode = imageTexture.alphaMode;
                detail.initialized = true;
            });

        // Revisions are the non-destructive producer/consumer signal. Keep the
        // legacy Component::dirty queues bounded for callers that still use them.
        ImageTexture::eachDirty(r, [](entt::entity) {});
        TextureResource::eachDirty(r, [](entt::entity) {});
    });

    Inherit::update(vsgcontext);
}
