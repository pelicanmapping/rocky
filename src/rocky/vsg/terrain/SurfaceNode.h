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
#include <rocky/Rendering.h>

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
        void setElevation(Image::Ptr raster, const glm::fmat4& scaleBias);

        //! Elevation raster representing this surface
        std::shared_ptr<Image> getElevationRaster() const {
            return _elevationRaster;
        }

        //! Elevation matrix representing to this surface
        const glm::fmat4& getElevationMatrix() const {
            return _elevationMatrix;
        }
        
        //! World-space visibility check (includes bounding box
        //! and horizon checks)
        inline bool isVisible(vsg::RecordTraversal& rv) const;

        //! Force a recompute of the bounding box and culling information
        const vsg::dsphere& recomputeBound();

        vsg::dsphere worldBoundingSphere;
        vsg::dbox localbbox;

    public:

        inline void traverse(vsg::ConstVisitor& v) const override
        {
            if (_proxyGeom)
            {
                if (auto i = v.cast<vsg::Intersector>())
                {                    
                    i->apply(*_proxyGeom);
                    return;
                }
            }
            Inherit::traverse(v);
        }

    protected:

        TileKey _tilekey;
        int _lastFramePassedCull = 0;
        std::shared_ptr<Image> _elevationRaster;
        glm::fmat4 _elevationMatrix;
        std::vector<vsg::dvec3> _worldPoints;
        bool _boundsDirty = true;
        vsg::dvec3 _horizonCullingPoint;
        bool _horizonCullingPoint_valid = false;
        vsg::ref_ptr<vsg::Geometry> _proxyGeom;
        vsg::ref_ptr<vsg::vec3Array> _proxyVerts;
        mutable ViewLocal<Horizon>* _horizon = nullptr;
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
        if (!_horizon)
        {
            rv.getValue("rocky.horizon", _horizon);
        }

        if (_horizon)
        {
            auto viewID = rv.getCommandBuffer()->viewID;
            auto& horizon = (*_horizon)[viewID];

            // Use the bounding sphere radius as margin for more conservative culling
            double cullRadius = worldBoundingSphere.radius;

            if (_horizonCullingPoint_valid)
            {
                return horizon.isVisible(_horizonCullingPoint, cullRadius);
            }
            else
            {
                for (int p = 0; p < 4; ++p)
                {
                    auto& wp = _worldPoints[p];
                    if (horizon.isVisible(wp.x, wp.y, wp.z, cullRadius))
                        return true;
                }
                return false;
            }
        }

        return true;
    }
}
