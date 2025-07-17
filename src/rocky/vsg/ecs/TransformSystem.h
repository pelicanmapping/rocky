/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/System.h>
#include <rocky/Callbacks.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS System that processes Transform and TransformDetail components.
    */
    class ROCKY_EXPORT TransformSystem : public vsg::Inherit<vsg::Node, TransformSystem>, public System
    {
    public:
        //! Construct the system
        TransformSystem(Registry& r);

        void update(VSGContext& runtime) override;

        //! Called periodically to update the transforms
        void traverse(vsg::RecordTraversal& record) const override;

        //! Callback to invoke if the update/traverse resulted in any changes
        Callback<> onChanges;
    };
}
