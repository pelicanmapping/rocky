/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/ecs/Transform.h>
#include <rocky/Rendering.h>
#include <rocky/SRS.h>
#include <rocky/Ellipsoid.h>
#include <rocky/Horizon.h>

namespace ROCKY_NAMESPACE
{
    //! Internal data calculated from a Transform instance in the context of a specific camera.
    struct TransformViewDetail
    {
        int revision = -1;    // revision of this data, for syncing
        vsg::dmat4 model;     // model matrix
        vsg::dmat4 proj;      // projection matrix
        vsg::dmat4 modelview; // modelview matrix
        vsg::dmat4 mvp;       // modelview-projection matrix
        vsg::vec4 viewport;   // pixel-space viewport
        bool passingCull = true; // whether the transform passes frustum/horizon culling
    };

    //! Per-VSG-view TransformViewData.
    //! This is an ECS component that the TransformSystem will automatically 
    //! attach to each entity that has a Transform component.
    struct TransformDetail
    {
        //! Construct the object, and force the sychronization Transform to be dirty.
        TransformDetail() {
            sync.revision = -1;
        }

        // Synchronous copy of the corresponding Transform component.
        // TransformSystem will sync this as necessary so that the user can
        // safely and frame-accurately perform asynchronous Transform updates.
        Transform sync;

        // Per-view data, calculated during the record traversal
        ViewLocal<TransformViewDetail> views;

        // Cached global data
        struct Cached
        {
            SRS world_srs;
            const Ellipsoid* world_ellipsoid = nullptr;
            SRSOperation pos_to_world;
            ViewLocal<Horizon>* horizon = nullptr;
        };
        Cached cached;

        //! Updates the per-view data for the given record traversal.
        //! Return true if any updates were made due to a dirty Transform.
        bool update(vsg::RecordTraversal&);

        //! Push the matrix associated with this transform onto the record stack
        void push(vsg::RecordTraversal&) const;

        //! Pop a matrix recorded with push(...)
        void pop(vsg::RecordTraversal&) const;

        //! True if this transform is visible in the provided view state
        inline bool passingCull(RenderingState) const;
    };


    // inline functions
    inline bool TransformDetail::passingCull(RenderingState rs) const
    {
        return views[rs.viewID].passingCull;
    }
}
