/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SurfaceNode.h"
#include "Utils.h"
#include "GeometryPool.h"
#include <rocky/Horizon.h>
#include <numeric>

using namespace rocky;

#define LC "[SurfaceNode] "

void
HorizonTileCuller::set(
    shared_ptr<SRS> srs,
    const vsg::dmat4& local2world,
    const vsg::dbox& bbox)
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
        double zMin = std::min(bbox.min.z, 0.0);

        zMin = std::max(zMin, -25000.0); // approx the lowest point on earth * 2

        _horizon->setEllipsoid(Ellipsoid(
            srs->getEllipsoid().getRadiusEquator() + zMin,
            srs->getEllipsoid().getRadiusPolar() + zMin));

        dmat4 m = to_glm(local2world);

        _points[0] = m * dvec3(bbox.min.x, bbox.min.y, bbox.max.z);
        _points[1] = m * dvec3(bbox.max.x, bbox.min.y, bbox.max.z);
        _points[2] = m * dvec3(bbox.max.x, bbox.max.y, bbox.max.z);
        _points[3] = m * dvec3(bbox.min.x, bbox.max.y, bbox.max.z);
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

SurfaceNode::SurfaceNode(const TileKey& tilekey)
{
    _tileKey = tilekey;

    // Establish a local reference frame for the tile:
    GeoPoint centroid = tilekey.getExtent().getCentroid();

    dmat4 local2world;
    centroid.createLocalToWorld( local2world );
    this->matrix = to_vsg(local2world);
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


void
SurfaceNode::recomputeLocalBBox()
{
    _localbbox = vsg::dbox();

    if (children.empty())
        return;

    ROCKY_TODO("compute the local bounding box for a surface node (take from TileDrawable)");

    auto geom = static_cast<vsg::Group*>(children[0].get())->children[0]->cast<SharedGeometry>();
    ROCKY_SOFT_ASSERT_AND_RETURN(geom, void());

    auto verts = geom->proxy_verts;
    auto normals = geom->proxy_normals;
    auto uvs = geom->proxy_uvs;
    
    ROCKY_SOFT_ASSERT_AND_RETURN(verts && normals && uvs, void());

    if (_proxyMesh.size() < verts->size())
    {
        _proxyMesh.resize(verts->size());
    }

    if (_elevationRaster)
    {
        float
            scaleU = _elevationMatrix[0][0],
            scaleV = _elevationMatrix[1][1],
            biasU = _elevationMatrix[3][0],
            biasV = _elevationMatrix[3][1];

        ROCKY_SOFT_ASSERT_AND_RETURN(!equivalent(scaleU, 0.0f) && equivalent(scaleV, 0.0f), void());

        for (int i = 0; i < verts->size(); ++i)
        {
            if (((int)uvs->at(i).z & VERTEX_HAS_ELEVATION) == 0)
            {
                float h = _elevationRaster->data<float>(
                    clamp(uvs->at(i).x*scaleU + biasU, 0.0f, 1.0f),
                    clamp(uvs->at(i).y*scaleV + biasV, 0.0f, 1.0f));

                _proxyMesh[i] = verts->at(i) + normals->at(i) * h;
            }
            else
            {
                _proxyMesh[i] = verts->at(i);
            }
        }
    }

    else
    {
        std::copy(verts->begin(), verts->end(), _proxyMesh.begin());
    }

    for (auto& vert : _proxyMesh)
    {
        _localbbox.add(vert);
    }
}

void
SurfaceNode::setElevation(
    shared_ptr<Image> raster,
    const dmat4& scaleBias)
{
    _elevationRaster = raster;
    _elevationMatrix = scaleBias;
    recomputeBound();
}

#define corner(N) vsg::dvec3( \
    (N & 0x1) ? _localbbox.max.x : _localbbox.min.x, \
    (N & 0x2) ? _localbbox.max.y : _localbbox.min.y, \
    (N & 0x4) ? _localbbox.max.z : _localbbox.min.z)

void
SurfaceNode::recomputeBound()
{
    // next compute the bounding box in local space:
    recomputeLocalBBox();

    // Compute the medians of each potential child node:

    vsg::dvec3 minZMedians[4];
    vsg::dvec3 maxZMedians[4];

    minZMedians[0] = (corner(0) + corner(1))*0.5;
    minZMedians[1] = (corner(1) + corner(3))*0.5;
    minZMedians[2] = (corner(3) + corner(2))*0.5;
    minZMedians[3] = (corner(0) + corner(2))*0.5;

    maxZMedians[0] = (corner(4) + corner(5))*0.5;
    maxZMedians[1] = (corner(5) + corner(7))*0.5;
    maxZMedians[2] = (corner(7) + corner(6))*0.5;
    maxZMedians[3] = (corner(4) + corner(6))*0.5;

    // Child 0 corners
    _childrenCorners[0][0] = (corner(0));
    _childrenCorners[0][1] = minZMedians[0];
    _childrenCorners[0][2] = minZMedians[3];
    _childrenCorners[0][3] = (minZMedians[0] + minZMedians[2])*0.5;

    _childrenCorners[0][4] = (corner(4));
    _childrenCorners[0][5] = maxZMedians[0];
    _childrenCorners[0][6] = maxZMedians[3];
    _childrenCorners[0][7] = (maxZMedians[0] + maxZMedians[2])*0.5;

    // Child 1 corners
    _childrenCorners[1][0] = minZMedians[0];
    _childrenCorners[1][1] = (corner(1));
    _childrenCorners[1][2] = (minZMedians[0] + minZMedians[2])*0.5;
    _childrenCorners[1][3] = minZMedians[1];

    _childrenCorners[1][4] = maxZMedians[0];
    _childrenCorners[1][5] = (corner(5));
    _childrenCorners[1][6] = (maxZMedians[0] + maxZMedians[2])*0.5;
    _childrenCorners[1][7] = maxZMedians[1];

    // Child 2 corners
    _childrenCorners[2][0] = minZMedians[3];
    _childrenCorners[2][1] = (minZMedians[0] + minZMedians[2])*0.5;
    _childrenCorners[2][2] = (corner(2));
    _childrenCorners[2][3] = minZMedians[2];

    _childrenCorners[2][4] = maxZMedians[3];
    _childrenCorners[2][5] = (maxZMedians[0] + maxZMedians[2])*0.5;
    _childrenCorners[2][6] = (corner(6));
    _childrenCorners[2][7] = maxZMedians[2];

    // Child 3 corners
    _childrenCorners[3][0] = (minZMedians[0] + minZMedians[2])*0.5;
    _childrenCorners[3][1] = minZMedians[1];
    _childrenCorners[3][2] = minZMedians[2];
    _childrenCorners[3][3] = (corner(3));

    _childrenCorners[3][4] = (maxZMedians[0] + maxZMedians[2])*0.5;
    _childrenCorners[3][5] = maxZMedians[1];
    _childrenCorners[3][6] = maxZMedians[2];
    _childrenCorners[3][7] = (corner(7));

    // Transform the child corners to world space
    
    for (int i = 0; i < 4; ++i)
    {
        VectorPoints& childCorners = _childrenCorners[i];
        for (int j = 0; j < 8; ++j)
        {
            childCorners[j] = childCorners[j] * this->matrix;
            _spheres[i * 8 + j].set(childCorners[j], 0.0f);
        }
    }

    //if (_enableDebugNodes)
    //{
    //    removeDebugNode();
    //    addDebugNode(box);
    //}

#if 1
    // Update the horizon culler.
    _horizonCuller.set(
        _tileKey.getProfile()->getSRS(),
        this->matrix, //local2world,
        _localbbox);
#endif

    // need this?
    //dirtyBound();

    auto center_local = (_localbbox.min + _localbbox.max) * 0.5;

    boundingSphere.set(
        center_local * this->matrix, // world coords
        length(corner(0) + corner(7)) * 0.5);
}
