/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SurfaceNode.h"
#include "Utils.h"
#include <rocky/Horizon.h>
#include <numeric>

using namespace rocky;

#define LC "[SurfaceNode] "

void
HorizonTileCuller::set(
    shared_ptr<SRS> srs,
    const dmat4& local2world,
    const Box& bbox)
{
    if (!_horizon && srs->isGeographic())
    {
        _horizon = std::make_shared<Horizon>();
    }

    if (_horizon)
    {
        _horizon->setEllipsoid(srs->getEllipsoid());

        // Adjust the horizon ellipsoid based on the minimum Z value of the tile;
        // necessary because a tile that's below the ellipsoid (ocean floor, e.g.)
        // may be visible even if it doesn't pass the horizon-cone test. In such
        // cases we need a more conservative ellipsoid.
        double zMin = std::min(bbox.zmin, 0.0);
        zMin = std::max(zMin, -25000.0); // approx the lowest point on earth * 2
        _horizon->setEllipsoid(Ellipsoid(
            srs->getEllipsoid().getRadiusEquator() + zMin,
            srs->getEllipsoid().getRadiusPolar() + zMin));

        // consider the uppermost 4 points of the tile-aligned bounding box.
        // (the last four corners of the bbox are the "zmax" corners.)
        //for (unsigned i = 0; i < 4; ++i)
        //{
        //    _points[i] = bbox.corner(4 + i) * local2world;
        //}

        _points[0] = dvec3(bbox.xmin, bbox.ymin, bbox.zmax);
        _points[1] = dvec3(bbox.xmax, bbox.ymin, bbox.zmax);
        _points[2] = dvec3(bbox.xmax, bbox.ymax, bbox.zmax);
        _points[3] = dvec3(bbox.xmin, bbox.ymax, bbox.zmax);
    }
}

bool
HorizonTileCuller::isVisible(const dvec3& from) const
{
    if (!_horizon)
        return true;

    // alternate method (slower)
    //return _horizon->isVisible(from, _bs.center(), _bs.radius());

    for (unsigned i = 0; i < 4; ++i)
        if (_horizon->isVisible(from, _points[i], 0.0))
            return true;

    return false;
}

//..............................................................


const bool SurfaceNode::_enableDebugNodes = ::getenv("OSGEARTH_REX_DEBUG") != 0L;

SurfaceNode::SurfaceNode(
    const TileKey& tilekey) //,
    //vsg::ref_ptr<TileDrawable> drawable)
{
    //setName(tilekey.str());
    _tileKey = tilekey;

    //_drawable = drawable;

    // Create the final node.
    //addChild(_drawable);

    // Establish a local reference frame for the tile:
    GeoPoint centroid = tilekey.getExtent().getCentroid();

    dmat4 local2world;
    centroid.createLocalToWorld( local2world );
    this->matrix = to_vsg(local2world);
    //setMatrix( local2world );
    
    // Initialize the cached bounding box.
    setElevationData(nullptr, fmat4(1));
}

vsg::dsphere
SurfaceNode::computeBound() const
{
    dmat4 local2world;

    //computeLocalToWorldMatrix(local2world, 0L); //??
    local2world = to_glm(this->matrix);

    Sphere bs;
    for (unsigned i = 0; i < 8; ++i)
        bs.expandBy(_localbbox.corner(i)*local2world);

    return vsg::dsphere(
        vsg::dvec3(bs.center.x, bs.center.y, bs.center.z),
        bs.radius);
}

#if 0
float
SurfaceNode::getPixelSizeOnScreen(osg::CullStack* cull) const
{     
    // By using the width, the "apparent" pixel size will decrease as we
    // near the poles.
    double R = _drawable->getWidth()*0.5;
    //double R = _drawable->getRadius() / 1.4142;
    return cull->clampedPixelSize(getMatrix().getTrans(), R) / cull->getLODScale();
}
#endif

void
SurfaceNode::setLastFramePassedCull(unsigned fn)
{
    _lastFramePassedCull = fn;
}


const Box&
SurfaceNode::recomputeLocalBBox()
{
    ROCKY_TODO("compute the local bounding box for a surface node (take from TileDrawable)");
    return _localbbox;
}

void
SurfaceNode::setElevationData(
    shared_ptr<Heightfield> raster,
    const dmat4& scaleBias)
{
    _elevationRaster = raster;
    _elevationMatrix = scaleBias;

    if ( !_drawable.valid() )
        return;

    // communicate the raster to the drawable:
    //_drawable->setElevationRaster(raster, scaleBias);

    // next compute the bounding box in local space:
    //auto& box = _drawable->getBoundingBox();
    auto& box = recomputeLocalBBox();

    // Compute the medians of each potential child node:

    vsg::vec3 minZMedians[4];
    vsg::vec3 maxZMedians[4];

    minZMedians[0] = to_vsg(box.corner(0)+box.corner(1))*0.5;
    minZMedians[1] = to_vsg(box.corner(1)+box.corner(3))*0.5;
    minZMedians[2] = to_vsg(box.corner(3)+box.corner(2))*0.5;
    minZMedians[3] = to_vsg(box.corner(0)+box.corner(2))*0.5;
                                  
    maxZMedians[0] = to_vsg(box.corner(4)+box.corner(5))*0.5;
    maxZMedians[1] = to_vsg(box.corner(5)+box.corner(7))*0.5;
    maxZMedians[2] = to_vsg(box.corner(7)+box.corner(6))*0.5;
    maxZMedians[3] = to_vsg(box.corner(4)+box.corner(6))*0.5;
                                  
    // Child 0 corners
    _childrenCorners[0][0] = to_vsg(box.corner(0));
    _childrenCorners[0][1] =  minZMedians[0];
    _childrenCorners[0][2] =  minZMedians[3];
    _childrenCorners[0][3] = (minZMedians[0]+minZMedians[2])*0.5f;

    _childrenCorners[0][4] = to_vsg(box.corner(4));
    _childrenCorners[0][5] =  maxZMedians[0];
    _childrenCorners[0][6] =  maxZMedians[3];
    _childrenCorners[0][7] = (maxZMedians[0]+maxZMedians[2])*0.5f;

    // Child 1 corners
    _childrenCorners[1][0] =  minZMedians[0];
    _childrenCorners[1][1] = to_vsg(box.corner(1));
    _childrenCorners[1][2] = (minZMedians[0]+minZMedians[2])*0.5f;
    _childrenCorners[1][3] =  minZMedians[1];
                     
    _childrenCorners[1][4] =  maxZMedians[0];
    _childrenCorners[1][5] = to_vsg(box.corner(5));
    _childrenCorners[1][6] = (maxZMedians[0]+maxZMedians[2])*0.5f;
    _childrenCorners[1][7] =  maxZMedians[1];

    // Child 2 corners
    _childrenCorners[2][0] =  minZMedians[3];
    _childrenCorners[2][1] = (minZMedians[0]+minZMedians[2])*0.5f;
    _childrenCorners[2][2] = to_vsg(box.corner(2));
    _childrenCorners[2][3] =  minZMedians[2];
                     
    _childrenCorners[2][4] =  maxZMedians[3];
    _childrenCorners[2][5] = (maxZMedians[0]+maxZMedians[2])*0.5f;
    _childrenCorners[2][6] = to_vsg(box.corner(6));
    _childrenCorners[2][7] =  maxZMedians[2]; 

    // Child 3 corners
    _childrenCorners[3][0] = (minZMedians[0]+minZMedians[2])*0.5f;
    _childrenCorners[3][1] =  minZMedians[1];
    _childrenCorners[3][2] =  minZMedians[2];
    _childrenCorners[3][3] = to_vsg(box.corner(3));
                     
    _childrenCorners[3][4] = (maxZMedians[0]+maxZMedians[2])*0.5f;
    _childrenCorners[3][5] =  maxZMedians[1];
    _childrenCorners[3][6] =  maxZMedians[2];
    _childrenCorners[3][7] = to_vsg(box.corner(7));

    // Transform the child corners to world space
    
    dmat4 local2world = to_glm(this->matrix);
    //const osg::Matrix& local2world = getMatrix();
    for (int i = 0; i < 4; ++i)
    {
        VectorPoints& childCorners = _childrenCorners[i];
        for (int j = 0; j < 8; ++j)
        {
            dvec4 corner = dvec4(to_glm(childCorners[j]), 1);
            corner = corner * local2world;
            childCorners[j] = to_vsg(fvec3(corner));
            //corner = to_vsg(dvec4(to_glm(corner),1) * local2world;
        }
    }

    //if (_enableDebugNodes)
    //{
    //    removeDebugNode();
    //    addDebugNode(box);
    //}

    // Update the horizon culler.
    _horizonCuller.set(
        _tileKey.getProfile()->getSRS(),
        local2world,
        box);

    // need this?
    //dirtyBound();
}

#if 0
void
SurfaceNode::addDebugNode(const osg::BoundingBox& box)
{
    _debugText = 0;
    _debugNode = makeBBox(box, _tileKey);
    addChild( _debugNode.get() );
}

void
SurfaceNode::removeDebugNode(void)
{
    _debugText = 0;
    if ( _debugNode.valid() )
    {
        removeChild( _debugNode.get() );
    }
}

void
SurfaceNode::setDebugText(const std::string& strText)
{
    if (_debugText.valid()==false)
    {
        return;
    }
    _debugText->setText(strText);
}
#endif
