/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/TileDrawable.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/vk/State.h>
#include <vsg/maths/vec3.h>

namespace rocky
{
    class Heightfield;
    class Horizon;
    class TileDrawable;

    class HorizonTileCuller
    {
    public:
        // four points representing to upper face of the bounding box
        dvec3 _points[4];

        // Horizon object to consider for culling
        shared_ptr<Horizon> _horizon;

        //! Reconfigure the culler
        void set(shared_ptr<SRS> srs, const dmat4& local2world, const Box& bbox);

        //! True if this tile may be visible relative to the horizon
        bool isVisible(const dvec3& from) const;
    };


    /**
     * SurfaceNode holds the geometry and transform information
     * for one terrain tile surface.
     */
    class SurfaceNode :
        public vsg::Inherit<vsg::MatrixTransform, SurfaceNode>
    {
    public:
        SurfaceNode(
            const TileKey& tilekey);

        //! Update the elevation raster associated with this tile
        void setElevation(
            shared_ptr<Heightfield> raster,
            const dmat4& scaleBias);

        //! Elevation raster representing this surface
        shared_ptr<Heightfield> getElevationRaster() const {
            return _elevationRaster;
        }

        //! Elevation matrix representing to this surface
        const dmat4& getElevationMatrix() const {
            return _elevationMatrix;
        }

        //! aligned BBOX for culling (derived from elevation raster)
        //const vsg::box& getAlignedBoundingBox() const {
        //    return _drawable->bound;
        //}
        
        //! Proxy drawable
        vsg::ref_ptr<TileDrawable> getDrawable() const { 
            return _drawable;
        }

        //! Horizon visibility check
        inline bool isVisibleFrom(const fvec3& viewpoint) const {
            return _horizonCuller.isVisible(viewpoint);
        }
        
        // A box can have 4 children. 
        // Returns true if any child box intersects the sphere of radius centered around point
        inline bool anyChildBoxIntersectsSphere(const vsg::vec3& point, float radiusSquared) {
            for(int c=0; c<4; ++c) {
                for(int j=0; j<8; ++j) {
                    if ( length2(_childrenCorners[c][j]-point) < radiusSquared )
                        return true;
                }
            }
            return false;
        }

        bool anyChildBoxWithinRange(float range, vsg::State* state) const {
            for(int c=0; c<4; ++c) {
                for(int j=0; j<8; ++j) {
                    if (state->lodDistance(vsg::sphere(_childrenCorners[c][j], 0.0f)) < range)
                        return true;
                    //if (nv.getDistanceToViewPoint(_childrenCorners[c][j], true) < range)
                        //return true;
                }
            }
            return false;
        }

        void setDebugText(const std::string& strText);

        vsg::dsphere computeBound() const;

        //osg::Node* getDebugNode() const { return _debugNode.get(); }

        void setLastFramePassedCull(unsigned fn);

        unsigned getLastFramePassedCull() const { return _lastFramePassedCull; }

        //float getPixelSizeOnScreen(osg::CullStack* cull) const;

    protected:

        TileKey _tileKey;
        vsg::ref_ptr<TileDrawable> _drawable;
        //vsg::ref_ptr<osg::Node> _debugNode;
        //vsg::ref_ptr<osgText::Text> _debugText;
        static const bool _enableDebugNodes;
        int _lastFramePassedCull;
        HorizonTileCuller _horizonCuller;

        shared_ptr<Heightfield> _elevationRaster;
        dmat4 _elevationMatrix;

        //void addDebugNode(const osg::BoundingBox& box);
        //void removeDebugNode(void);

        using VectorPoints = vsg::vec3[8];
        VectorPoints _worldCorners;

        using ChildrenCorners = VectorPoints[4];
        ChildrenCorners _childrenCorners;

        Box _localbbox;
        const Box& recomputeLocalBBox();

        std::vector<vsg::vec3> _proxyMesh;
    };

}
