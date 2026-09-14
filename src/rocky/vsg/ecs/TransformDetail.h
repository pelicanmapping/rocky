/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/ecs/Transform.h>
#include <rocky/ecs/PixelScale.h>
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
        vsg::dmat4 model;     // model matrix (possibly adjusted by dynamic scale)
        vsg::dmat4 baseModel; // model matrix before dynamic scale adjustment
        vsg::dmat4 baseModelWithoutScale; // model matrix with local scale removed
        vsg::dmat4 proj;      // projection matrix
        vsg::dmat4 modelview; // modelview matrix
        vsg::dmat4 mvp;       // modelview-projection matrix
        vsg::dvec4 position;  // view-space position after optional projection
        vsg::vec4 viewport;   // pixel-space viewport
        bool passingCull = true; // whether the transform passes frustum/horizon culling

        //! Test a sphere centered at the local origin against the MVP's frustum.
        //! Radius is in local units, before the scaling already included in mvp.
        inline bool passesFrustumCull(double radius) const;

        // Cached global data
        struct Cached
        {
            SRS world_srs;
            const Ellipsoid* world_ellipsoid = nullptr;
            SRSOperation pos_to_world;
            ViewLocal<Horizon>* horizon = nullptr;
        };
        Cached cache;
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

        //! Device pixel ratio as set by the TransformSystem; used for dynamic scaling
        float devicePixelRatio = 1.0;

        // Per-view data, calculated during the record traversal
        mutable ViewLocal<TransformViewDetail> views;

        //! Reset any cached data for the given view so the object
        //! can recalibrate itself after an SRS change (e.g.)
        void reset(std::uint32_t viewID);

        //! Updates the per-view data for the given record traversal.
        //! Return true if any updates were made due to a dirty Transform.
        bool traverse(vsg::RecordTraversal&, const PixelScale*);

        //! Push the matrix associated with this transform onto the record stack
        void push(vsg::RecordTraversal&) const;

        //! Pop a matrix recorded with push(...)
        void pop(vsg::RecordTraversal&) const;

        //! True if this transform is visible in the provided view state
        inline bool passingCull(RenderingState) const;
    };


    // inline functions
    inline bool TransformViewDetail::passesFrustumCull(double radius) const
    {
        // Pull the Vulkan clip planes (-w <= x,y <= w, 0 <= z <= w) back
        // into local coordinates. This includes all model/view scaling and
        // avoids a perspective divide when the sphere crosses the eye plane.
        const vsg::dvec4 x(mvp[0][0], mvp[1][0], mvp[2][0], mvp[3][0]);
        const vsg::dvec4 y(mvp[0][1], mvp[1][1], mvp[2][1], mvp[3][1]);
        const vsg::dvec4 z(mvp[0][2], mvp[1][2], mvp[2][2], mvp[3][2]);
        const vsg::dvec4 w(mvp[0][3], mvp[1][3], mvp[2][3], mvp[3][3]);
        const vsg::dvec4 planes[] = { w + x, w - x, w + y, w - y, z, w - z };

        radius = std::max(radius, 0.0);
        for (const auto& plane : planes)
        {
            // At the local origin, the plane equation evaluates to plane.w.
            // Reject only if the entire sphere lies outside this plane.
            const auto normalLength = vsg::length(vsg::dvec3(plane.x, plane.y, plane.z));
            if (plane.w < -radius * normalLength)
                return false;
        }
        return true;
    }

    inline bool TransformDetail::passingCull(RenderingState rs) const
    {
        return views[rs.viewID].passingCull;
    }
}
