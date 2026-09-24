/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Optics.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/Callbacks.h>
#include <rocky/TileKey.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <mutex>
#include <unordered_set>

namespace ROCKY_NAMESPACE::detail
{
    //! Internal per-view result of applying a ProjectedTexture placement policy.
    struct ProjectionViewDetail
    {
        glm::dvec3 focalPoint = glm::dvec3(0.0);
        double focalDistance = 1.0;
        double nearDistance = 1.0;
        double farDistance = 1.0;
        bool focalPointValid = false;

        // Terrain-intersection cache. The cache is invalidated by a changed
        // projector matrix or a loaded terrain tile containing the last hit.
        bool intersectionCacheValid = false;
        glm::dmat4 lastProjectorWorld = glm::dmat4(1.0);
        SRS lastWorldSRS;

        // Automatic overlay receiving volume, independent of its source Transform and texture/atlas fit.
        // The range is in the post-placement projector's normalized Z coordinates; XY remains unchanged.
        glm::dvec2 terrainDepthRange{ 0.0, 0.0 };
        glm::dmat4 depthProjectorWorld{ 1.0 };
        GeoExtent terrainDepthFootprint;
        bool terrainDepthRangeValid = false;
        bool terrainDepthCacheValid = false;

        //! Applies cached depth only to the exact projector it was queried for; preserves local XY/UV mapping.
        bool applyTerrainDepth(glm::dmat4& world) const
        {
            if (!terrainDepthRangeValid)
                return false;
            for (unsigned column = 0; column < 4u; ++column)
                for (unsigned row = 0; row < 4u; ++row)
                    if (world[column][row] != depthProjectorWorld[column][row])
                        return false;
            world[3] += world[2] * (0.5 * (terrainDepthRange.x + terrainDepthRange.y));
            world[2] *= terrainDepthRange.y - terrainDepthRange.x;
            return true;
        }
    };

    //! Internal per-view placement results for one ProjectedTexture instance.
    struct ProjectionDetail
    {
        ViewLocal<ProjectionViewDetail> views;
    };
}

namespace ROCKY_NAMESPACE
{
    /**
     * Resolves optical parameters and terrain placement for projected textures.
     */
    class ROCKY_EXPORT OpticsSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, OpticsSystemNode>
    {
    public:
        //! Construct the system
        OpticsSystemNode(Registry& registry);

        //! Target geometry used for projected-texture terrain intersections.
        vsg::observer_ptr<vsg::Node> target;

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    private:
        vsg::observer_ptr<vsg::Node> _subscribedTarget;
        CallbackSubs _terrainSubscriptions;
        bool _targetHasChangeNotifications = false;
        bool _targetChanged = true;
        std::mutex _loadedTilesMutex;
        std::unordered_set<TileKey> _loadedTiles;
        std::unordered_set<TileKey> _changedBoundsTiles;
        bool _terrainBoundsReset = false;

        //! Updates the terrain subscription when the target node changes.
        void updateTargetSubscription();

        //! Publishes manual/fallback values for standalone Optics components.
        void updateOptics(VSGContext);

        //! Computes effective terrain placement for normalized projections.
        void updateProjections(VSGContext);

        //! Creates runtime state for a newly added Optics component.
        void on_construct_Optics(entt::registry& r, entt::entity e);

        //! Removes runtime state owned by a removed Optics component.
        void on_destroy_Optics(entt::registry& r, entt::entity e);

        //! Creates runtime placement state for a projected texture.
        void on_construct_ProjectedTexture(entt::registry& r, entt::entity e);

        //! Removes runtime placement state owned by a projected texture.
        void on_destroy_ProjectedTexture(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::OpticsSystemNode)
