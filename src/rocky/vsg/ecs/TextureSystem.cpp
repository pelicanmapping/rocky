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
    //! One source cache per texture entity, independent of its consumers.
    struct TextureSourceDetail
    {
        Image::Ptr sourceImage;
        vsg::ref_ptr<vsg::ImageInfo> sourceTexture;
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
        r.on_construct<Texture>().connect<&TextureSystemNode::on_construct_Texture>(*this);
        r.on_update<Texture>().connect<&TextureSystemNode::on_update_Texture>(*this);
        r.on_destroy<Texture>().connect<&TextureSystemNode::on_destroy_Texture>(*this);
        r.on_construct<ImageTexture>().connect<&TextureSystemNode::on_construct_ImageTexture>(*this);
        r.on_update<ImageTexture>().connect<&TextureSystemNode::on_update_ImageTexture>(*this);
        r.on_destroy<ImageTexture>().connect<&TextureSystemNode::on_destroy_ImageTexture>(*this);
        r.on_destroy<TextureResource>().connect<&TextureSystemNode::on_destroy_TextureResource>(*this);

        auto dirtyEntity = r.create();
        r.emplace<Texture::Dirty>(dirtyEntity);
        r.emplace<ImageTexture::Dirty>(dirtyEntity);
        r.emplace<TextureResource::Dirty>(dirtyEntity);

        r.view<Texture>().each([&](auto entity, auto&) { on_construct_Texture(r, entity); });
        r.view<ImageTexture>().each([&](auto entity, auto&) { on_construct_ImageTexture(r, entity); });
    });
}

void TextureSystemNode::on_construct_Texture(entt::registry& r, entt::entity entity)
{
    Texture::dirty(r, entity);
    (void)r.get_or_emplace<TextureSourceDetail>(entity);
}

void TextureSystemNode::on_update_Texture(entt::registry& r, entt::entity entity)
{
    Texture::dirty(r, entity);
}

void TextureSystemNode::on_destroy_Texture(entt::registry& r, entt::entity entity)
{
    if (auto* resource = r.try_get<TextureResource>(entity);
        resource && resource->producer == TextureResourceProducer::Texture)
        r.remove<TextureResource>(entity);
    if (!r.any_of<ImageTexture>(entity))
        r.remove<TextureSourceDetail>(entity);
}

void TextureSystemNode::on_construct_ImageTexture(entt::registry& r, entt::entity entity)
{
    ImageTexture::dirty(r, entity);
    (void)r.get_or_emplace<TextureSourceDetail>(entity);
}

void TextureSystemNode::on_update_ImageTexture(entt::registry& r, entt::entity entity)
{
    ImageTexture::dirty(r, entity);
}

void TextureSystemNode::on_destroy_ImageTexture(entt::registry& r, entt::entity entity)
{
    if (auto* resource = r.try_get<TextureResource>(entity);
        resource && resource->producer == TextureResourceProducer::ImageTexture)
        r.remove<TextureResource>(entity);
    if (!r.any_of<Texture>(entity))
        r.remove<TextureSourceDetail>(entity);
}

void TextureSystemNode::on_destroy_TextureResource(entt::registry& r, entt::entity entity)
{
    // Hook the resource itself, not only its source: whole-entity destruction
    // may remove these components in either order. Other producers manage their
    // own resource lifetimes and must not be retired by this system.
    const auto& resource = r.get<TextureResource>(entity);
    if ((resource.producer == TextureResourceProducer::ImageTexture ||
        resource.producer == TextureResourceProducer::Texture) && resource.texture)
    {
        std::scoped_lock lock(_pendingDisposalsMutex);
        _pendingDisposals.emplace_back(resource.texture);
    }
}

void TextureSystemNode::update(VSGContext vsgcontext)
{
    if (!vsgcontext)
        return;

    // Consumers can relinquish their descriptor references without freeing
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
        r.view<TextureSourceDetail>().each(
            [&](auto entity, auto& detail)
            {
                const auto* native = r.try_get<Texture>(entity);
                const auto* imageTexture = r.try_get<ImageTexture>(entity);
                if (!native && !imageTexture)
                    return;

                auto* resource = r.try_get<TextureResource>(entity);
                // Never let system order choose between conflicting producers.
                // Withdraw only our own publication; the other producer owns its cache.
                if ((native && imageTexture) || r.any_of<RenderTexture>(entity))
                {
                    if (!detail.conflictLogged)
                        Log()->warn("TextureSystemNode: entity {} has multiple texture sources; use only one",
                            entt::to_integral(entity));
                    detail.conflictLogged = true;
                    detail.initialized = false;
                    detail.sourceImage.reset();
                    detail.sourceTexture = {};
                    if (resource && (resource->producer == TextureResourceProducer::ImageTexture ||
                        resource->producer == TextureResourceProducer::Texture))
                        r.remove<TextureResource>(entity);
                    return;
                }

                const auto producer = native ? TextureResourceProducer::Texture : TextureResourceProducer::ImageTexture;
                if (resource && resource->producer != producer)
                {
                    if (!detail.conflictLogged)
                    {
                        Log()->warn("TextureSystemNode: entity {} has a resource owned by another producer",
                            entt::to_integral(entity));
                        detail.conflictLogged = true;
                    }
                    return;
                }

                const auto sourceImage = imageTexture ? imageTexture->image : Image::Ptr{};
                const auto sourceTexture = native ? native->imageInfo : vsg::ref_ptr<vsg::ImageInfo>{};
                const auto sourceRevision = native ? native->componentRevision() : imageTexture->componentRevision();
                const auto origin = native ? native->origin : imageTexture->origin;
                const auto alphaMode = native ? native->alphaMode : imageTexture->alphaMode;
                detail.conflictLogged = false;
                if (detail.initialized && resource &&
                    detail.sourceImage == sourceImage && detail.sourceTexture == sourceTexture &&
                    detail.sourceRevision == sourceRevision && detail.origin == origin && detail.alphaMode == alphaMode)
                    return;

                if (!resource)
                {
                    resource = &r.emplace<TextureResource>(entity);
                    resource->producer = producer;
                    resource->ready = false;
                }

                if (resource->texture && resource->texture != sourceTexture)
                    dispose(resource->texture);

                resource->owner = entity;
                resource->texture = {};
                resource->ready = false;
                resource->origin = origin;
                resource->alphaMode = alphaMode;
                ++resource->revision;
                resource->dirty(r);

                if (native)
                {
                    // Share the caller's image/sampler; do not copy it per consumer.
                    resource->texture = sourceTexture;
                    resource->ready = true;
                    if (detail.initialized && sourceTexture && detail.sourceTexture == sourceTexture &&
                        detail.sourceRevision != sourceRevision)
                        requestUpload(sourceTexture);
                }
                else if (sourceImage)
                {
                    auto image = wrapImageInVSG(sourceImage);
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

                detail.sourceImage = sourceImage;
                detail.sourceTexture = sourceTexture;
                detail.sourceRevision = sourceRevision;
                detail.origin = origin;
                detail.alphaMode = alphaMode;
                detail.initialized = true;
            });

        // Revisions are the non-destructive producer/consumer signal. Keep the
        // legacy Component::dirty queues bounded for callers that still use them.
        ImageTexture::eachDirty(r, [](entt::entity) {});
        Texture::eachDirty(r, [](entt::entity) {});
        TextureResource::eachDirty(r, [](entt::entity) {});
    });

    Inherit::update(vsgcontext);
}
