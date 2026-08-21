/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/SRS.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <cstdint>
#include <mutex>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * Experimental vector-only overlay encoder. Each supported Line/Point
     * payload receives an independently owned Slughorn atlas pair.
     */
    class ROCKY_EXPORT SlugSystemNode :
        public vsg::Inherit<detail::SimpleSystemNodeBase, SlugSystemNode>
    {
    public:
        SlugSystemNode(Registry& registry);
        void update(VSGContext) override;

        //! World SRS used to convert georeferenced geometry into projector space.
        SRS worldSRS;

        //! Initial per-overlay atlas width. The adapter grows it when necessary.
        static constexpr std::uint32_t textureWidth = 512u;

        //! Experimental: merge consecutive segment contours whose mapped
        //! endpoints match exactly before asking Slughorn to stroke them.
        bool mergeConnectedLineSegments = true;

    private:
        // SlugResource components can disappear outside this system's update
        // (for example when an EntityNode is paged out). Hold their images until
        // update() can submit them to Rocky's deferred GPU disposer.
        std::mutex _pendingDisposalsMutex;
        std::vector<vsg::ref_ptr<vsg::ImageInfo>> _pendingDisposals;
        vsg::ref_ptr<vsg::Sampler> _atlasSampler;

        void on_destroy_SlugResource(entt::registry&, entt::entity);
    };
}

EVSG_type_name(rocky::SlugSystemNode)
