/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Optics.h>
#include <rocky/Callbacks.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <atomic>

namespace ROCKY_NAMESPACE
{
    /**
     * ECS system that handles Optics components
     */
    class ROCKY_EXPORT OpticsSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, OpticsSystemNode>
    {
    public:
        //! Construct the system
        OpticsSystemNode(Registry& registry);

        //! Target geometry for decals
        vsg::observer_ptr<vsg::Node> target;

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    private:
        std::atomic_uint64_t _terrainRevision = 1u;
        vsg::observer_ptr<vsg::Node> _subscribedTarget;
        CallbackSubs _terrainSubscriptions;
        bool _targetHasChangeNotifications = false;

        void updateTargetSubscription();
        void updateOptics(VSGContext);

        void on_construct_Optics(entt::registry& r, entt::entity e);
        void on_destroy_Optics(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::OpticsSystemNode)
