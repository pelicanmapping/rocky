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
        int revision = -1;    // revision of this data, for syncing
        vsg::dmat4 model;     // model matrix
        vsg::dmat4 proj;      // projection matrix
        vsg::dmat4 modelview; // modelview matrix
        vsg::dmat4 mvp;       // modelview-projection matrix
        vsg::vec4 viewport;   // pixel-space viewport
    };

    //! Per-VSG-view TransformViewData.
    //! This is an ECS component that the TransformSystem will automatically 
    //! attach to each entity that has a Transform component.
    struct TransformData
    {
        // Bare pointer to the coupled Transform component.
        // This requires EnTT pointer stabitily activation for Transform.
        Transform* transform = nullptr;

        // Per-view data, calculated during the record traversal
        detail::ViewLocal<TransformViewData> views;

        // Cached global data
        SRS world_srs;
        const Ellipsoid* world_ellipsoid = nullptr;
        SRSOperation pos_to_world;
        std::shared_ptr<Horizon> horizon;

        void update(vsg::RecordTraversal&);

        bool passesCull(vsg::RecordTraversal&) const;

        bool passesCull(std::uint32_t viewID) const;

        void push(vsg::RecordTraversal&) const;

        void pop(vsg::RecordTraversal&) const;
    };
}
