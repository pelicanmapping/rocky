/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/ViewLocal.h>
#include <rocky/vsg/ecs/Transform.h>
#include <rocky/SRS.h>
#include <rocky/Ellipsoid.h>
#include <rocky/Horizon.h>

namespace ROCKY_NAMESPACE
{
    //! Internal data calculated from a Transform instance in the context of a specific camera.
    struct TransformViewData
    {
        Transform* transform = nullptr; // pointer to the Transform component
        int revision = -1;    // revision of this data for syncing
        vsg::dmat4 local;     // local rotation/offset
        vsg::dmat4 model;     // model matrix
        vsg::mat4 proj;       // projection matrix
        vsg::mat4 modelview;  // modelview matrix
        vsg::vec4 viewport;
        SRS world_srs;
        bool culled = false;
        const Ellipsoid* world_ellipsoid = nullptr;
        SRSOperation pos_to_world;
        std::shared_ptr<Horizon> horizon;

        void update(vsg::RecordTraversal&);

        bool passesCull() const;

        void push(vsg::RecordTraversal&) const;

        void pop(vsg::RecordTraversal&) const;

    };

    //! Per-VSG-view TransformViewData.
    //! This is an ECS component that the TransformSystem will automatically 
    //! attach to each entity that has a Transform component.
    using TransformData = detail::ViewLocal<TransformViewData>;
}
