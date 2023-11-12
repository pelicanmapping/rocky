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
#include <rocky/vsg/engine/Utils.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/vk/State.h>
#include <vsg/maths/vec3.h>

namespace ROCKY_NAMESPACE
{
    class Horizon;
    class Runtime;

    /**
     * SurfaceNode holds the geometry and transform information
     * for one terrain tile surface.
     */
    class SurfaceNode : public vsg::Inherit<vsg::MatrixTransform, SurfaceNode>
    {
    public:
        SurfaceNode(
            const TileKey& tilekey,
            const SRS& worldSRS,
            Runtime& runtime);

        //! Update the elevation raster associated with this tile
        void setElevation(
            shared_ptr<Image> raster,
            const glm::dmat4& scaleBias);

        //! Elevation raster representing this surface
        shared_ptr<Image> getElevationRaster() const {
            return _elevationRaster;
        }

        //! Elevation matrix representing to this surface
        const glm::dmat4& getElevationMatrix() const {
            return _elevationMatrix;
        }
        
        //! World-space visibility check (includes bounding box
        //! and horizon checks)
        inline bool isVisible(vsg::State* state) const;
     
#if 0
        // A box can have 4 children. 
        // Returns true if any child box intersects the sphere of radius centered around point
        inline bool anyChildBoxIntersectsSphere(const vsg::dvec3& point, float radiusSquared) {
            for(int c=0; c<4; ++c) {
                for(int j=0; j<8; ++j) {
                    if ( length2(_childrenCorners[c][j]-point) < radiusSquared )
                        return true;
                }
            }
            return false;
        }
#endif

        bool anyChildBoxWithinRange(float range, vsg::State* state) const
        {
            for (unsigned i = 0u; i < 18u; ++i) {
                if (distanceTo(_worldPoints[i], state) <= range)
                    return true;
            }
            return false;
        }

        void recomputeBound();

        //float getPixelSizeOnScreen(osg::CullStack* cull) const;

        vsg::dsphere worldBoundingSphere;

    protected:

        TileKey _tileKey;
        int _lastFramePassedCull;
        shared_ptr<Image> _elevationRaster;
        glm::dmat4 _elevationMatrix;
        std::vector<vsg::dvec3> _worldPoints;
        vsg::dbox _localbbox;
        bool _boundsDirty;
        Runtime& _runtime;
        std::vector<vsg::vec3> _proxyMesh;
    };


    bool SurfaceNode::isVisible(vsg::State* state) const
    {
        // bounding box visibility check; this is much tighter than the bounding
        // sphere. _frustumStack.top() contains the frustum in world coordinates.
        // https://github.com/vsg-dev/VulkanSceneGraph/blob/master/include/vsg/vk/State.h#L267
        // The first 8 points in _worldPoints are the 8 corners of the bounding box
        // in world coordinates.
        // Note: POLYTOPE_SIZE is defined in vsg plane.h
        auto& frustum = state->_frustumStack.top();
        int p;
        for (int f = 0; f < POLYTOPE_SIZE; ++f) {
            for (p = 0; p < 8; ++p)
                if (vsg::distance(frustum.face[f], _worldPoints[p]) > 0.0) // visible?
                    break;
            if (p == 8)
                return false;
        }

        // still good? check against the horizon.
        shared_ptr<Horizon> horizon;
        if (state->getValue("horizon", horizon))
        {
            for (p = 0; p < 4; ++p)
            {
                auto& wp = _worldPoints[p];
                if (horizon->isVisible(wp.x, wp.y, wp.z))
                    return true;
            }

            return false;
        }
        else
        {
            return true;
        }
    }
}
