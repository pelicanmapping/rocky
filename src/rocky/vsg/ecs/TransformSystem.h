/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/System.h>
#include <rocky/SRS.h>
#include <rocky/Callbacks.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS System that processes Transform and TransformDetail components.
    */
    class ROCKY_EXPORT TransformSystemNode : public vsg::Inherit<vsg::Node, TransformSystemNode>, public System
    {
    public:
        //! Construct the system
        TransformSystemNode(Registry& r);

        void update(VSGContext vsgcontext) override;

        //! Called periodically to update the transforms
        void traverse(vsg::RecordTraversal& record) const override;

        vsg::Node* renderTextureParticipant() override { return this; }

        //! Callback to invoke if the update/traverse resulted in any changes
        Callback<> onChanges;

    private:
        struct ViewDetail
        {
            SRS worldSRS;
        };

        // Per-view data, calculated during the record traversal
        mutable ViewLocal<ViewDetail> views;


        void on_construct_Transform(entt::registry& r, entt::entity e);
        void on_update_Transform(entt::registry& r, entt::entity e);
        void on_destroy_Transform(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::TransformSystemNode)
