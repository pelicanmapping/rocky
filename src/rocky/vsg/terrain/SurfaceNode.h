/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/Image.h>
#include <rocky/SRS.h>
#include <rocky/TileKey.h>
#include <rocky/Horizon.h>
#include <rocky/vsg/Utils.h>
#include <rocky/vsg/ViewLocal.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/vk/State.h>
#include <vsg/maths/vec3.h>

namespace ROCKY_NAMESPACE
{
    /**
     * SurfaceNode holds the geometry and transform information
     * for one terrain tile surface.
     */
    class SurfaceNode : public vsg::Inherit<vsg::MatrixTransform, SurfaceNode>
    {
    public:
        SurfaceNode(const TileKey& tilekey, const SRS& worldSRS);

        //! Update the elevation raster associated with this tile
        void setElevation(
            std::shared_ptr<Image> raster,
            const glm::dmat4& scaleBias);

        //! Elevation raster representing this surface
        std::shared_ptr<Image> getElevationRaster() const {
            return _elevationRaster;
        }

        //! Elevation matrix representing to this surface
        const glm::dmat4& getElevationMatrix() const {
            return _elevationMatrix;
        }
        
        //! World-space visibility check (includes bounding box
        //! and horizon checks)
        inline bool isVisible(vsg::RecordTraversal& rv) const;

        //! Force a recompute of the bounding box and culling information
        const vsg::dsphere& recomputeBound();

        vsg::dsphere worldBoundingSphere;
        vsg::dbox localbbox;

    protected:

        TileKey _tilekey;
        int _lastFramePassedCull = 0;
        std::shared_ptr<Image> _elevationRaster;
        glm::dmat4 _elevationMatrix;
        std::vector<vsg::dvec3> _worldPoints;
        bool _boundsDirty = true;
        std::vector<vsg::vec3> _proxyMesh;
        vsg::dvec3 _horizonCullingPoint;
        bool _horizonCullingPoint_valid = false;

        struct ViewLocalData {
            std::shared_ptr<Horizon> horizon;
        };
        mutable detail::ViewLocal<ViewLocalData> _viewlocal;
    };


    inline bool SurfaceNode::isVisible(vsg::RecordTraversal& rv) const
    {
        auto* state = rv.getState();

        if (_worldPoints.size() < 8)
            return false;

        // bounding box visibility check; this is much tighter than the bounding
        // sphere. _frustumStack.top() contains the frustum in world coordinates.
        // https://github.com/vsg-dev/VulkanSceneGraph/blob/master/include/vsg/vk/State.h#L267
        // The first 8 points in _worldPoints are the 8 corners of the bounding box
        // in world coordinates.
        // Note: POLYTOPE_SIZE is defined in vsg plane.h
        auto& frustum = state->_frustumStack.top();
        int p;
        for (int f = 0; f < POLYTOPE_SIZE; ++f)
        {
            for (p = 0; p < 8; ++p)
                if (vsg::distance(frustum.face[f], _worldPoints[p]) > 0.0) // visible?
                    break;

            if (p == 8)
                return false;
        }

        // Horizon culling:
        auto& viewlocal = _viewlocal[state->_commandBuffer->viewID];
        if (!viewlocal.horizon)
        {
            rv.getValue("rocky.horizon", viewlocal.horizon);
        }
        if (viewlocal.horizon)
        {
            if (_horizonCullingPoint_valid)
            {
                return viewlocal.horizon->isVisible(_horizonCullingPoint);
            }
            else
            {
                for (int p = 0; p < 4; ++p)
                {
                    auto& wp = _worldPoints[p];
                    if (viewlocal.horizon->isVisible(wp.x, wp.y, wp.z))
                        return true;
                }
                return false;
            }
        }

        return true;
    }
}
