/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/TextureResource.h>
#include <mutex>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /** Publishes CPU ImageTexture components as renderer texture resources. */
    class ROCKY_EXPORT TextureSystemNode :
        public vsg::Inherit<detail::SimpleSystemNodeBase, TextureSystemNode>
    {
    public:
        TextureSystemNode(Registry& registry);
        void update(VSGContext) override;

    private:
        struct ImageTextureDetail
        {
            Image::Ptr sourceImage;
            std::uint64_t sourceRevision = 0u;
            TextureOrigin origin = TextureOrigin::LowerLeft;
            TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
            bool initialized = false;
            bool conflictLogged = false;
        };

        // Resource removal can occur on a paging thread. Retain producer-owned
        // images until update() can hand them to the deferred GPU disposer.
        std::mutex _pendingDisposalsMutex;
        std::vector<vsg::ref_ptr<vsg::ImageInfo>> _pendingDisposals;

        void on_construct_ImageTexture(entt::registry&, entt::entity);
        void on_destroy_ImageTexture(entt::registry&, entt::entity);
        void on_destroy_TextureResource(entt::registry&, entt::entity);
    };
}

EVSG_type_name(rocky::TextureSystemNode)
