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

namespace ROCKY_NAMESPACE
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
    };

    //! Internal per-view placement results for one ProjectedTexture instance.
    struct ProjectionDetail
    {
        ViewLocal<ProjectionViewDetail> views;
    };

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
