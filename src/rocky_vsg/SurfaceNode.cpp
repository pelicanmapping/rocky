/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SurfaceNode.h"
#include "Utils.h"
#include "GeometryPool.h"
#include "RuntimeContext.h"
#include <rocky/Heightfield.h>
#include <rocky/Horizon.h>
#include <numeric>
#include <vsg/utils/Builder.h>
#include <vsg/io/Options.h>

using namespace ROCKY_NAMESPACE;

#define LC "[SurfaceNode] "

// uncomment to draw each tile's tight bounding box
//#define RENDER_TILE_BBOX

void
HorizonTileCuller::set(
    const SRS& srs,
    const vsg::dmat4& local2world,
    const vsg::dbox& bbox)
{
    if (!_horizon && srs.isGeographic())
    {
        _horizon = std::make_shared<Horizon>();
    }

    if (_horizon)
    {
        _horizon->setEllipsoid(srs.ellipsoid());

        // Adjust the horizon ellipsoid based on the minimum Z value of the tile;
        // necessary because a tile that's below the ellipsoid (ocean floor, e.g.)
        // may be visible even if it doesn't pass the horizon-cone test. In such
        // cases we need a more conservative ellipsoid.
        double zMin = std::min(bbox.min.z, 0.0);

        zMin = std::max(zMin, -25000.0); // approx the lowest point on earth * 2

        _horizon->setEllipsoid(Ellipsoid(
            srs.ellipsoid().semiMajorAxis() + zMin,
            srs.ellipsoid().semiMinorAxis() + zMin));

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


SurfaceNode::SurfaceNode(const TileKey& tilekey, const SRS& worldSRS, RuntimeContext& runtime) :
    _tileKey(tilekey),
    _runtime(runtime),
    _boundsDirty(true)
{
    // Establish a local reference frame for the tile:
    GeoPoint centroid = tilekey.extent().getCentroid();
    centroid.transformInPlace(worldSRS);

    dmat4 local2world = worldSRS.localToWorldMatrix(centroid.to_dvec3());

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

void
SurfaceNode::setLastFramePassedCull(unsigned fn)
{
    _lastFramePassedCull = fn;
}
#endif

#define corner(N) vsg::dvec3( \
    (N & 0x1) ? _localbbox.max.x : _localbbox.min.x, \
    (N & 0x2) ? _localbbox.max.y : _localbbox.min.y, \
    (N & 0x4) ? _localbbox.max.z : _localbbox.min.z)

void
SurfaceNode::recomputeBound()
{
    if (!_boundsDirty)
        return;
    else
        _boundsDirty = false;

    _localbbox = vsg::dbox(
        { FLT_MAX, FLT_MAX, FLT_MAX },
        { -FLT_MAX, -FLT_MAX, -FLT_MAX });

    if (children.empty())
        return;

    auto geom = static_cast<vsg::Group*>(children.front().get())->children.front()->cast<SharedGeometry>();
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
        // yes, this is safe...for now :)
        auto heightfield = Heightfield::cast_from(_elevationRaster.get());

        double
            scaleU = _elevationMatrix[0][0],
            scaleV = _elevationMatrix[1][1],
            biasU = _elevationMatrix[3][0],
            biasV = _elevationMatrix[3][1];

        ROCKY_SOFT_ASSERT_AND_RETURN(!equiv(scaleU, 0.0) && !equiv(scaleV, 0.0), void());

        for (int i = 0; i < verts->size(); ++i)
        {
            if (((int)uvs->at(i).z & VERTEX_HAS_ELEVATION) == 0)
            {
                float h = heightfield->heightAtUV(
                    clamp(uvs->at(i).x * scaleU + biasU, 0.0, 1.0),
                    clamp(uvs->at(i).y * scaleV + biasV, 0.0, 1.0),
                    Heightfield::NEAREST);

                auto& v = verts->at(i);
                auto& n = normals->at(i);
                _proxyMesh[i] = v + n * h;
            }
            else
            {
                _proxyMesh[i] = (*verts)[i];
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

    auto& m = this->matrix;

    vsg::dvec3 center = m * ((_localbbox.min + _localbbox.max) * 0.5);
    double radius = 0.5 * vsg::length(_localbbox.max - _localbbox.min);

    worldBoundingSphere.set(center, radius);

    // Compute the medians of each potential child node:
    // Top points go first since these are the most likely to be visible
    // during the isVisible check.
    _worldPoints = {

        // top:
        m * corner(4),
        m * corner(5),
        m * corner(6),
        m * corner(7),

        // bottom:
        m * corner(0),
        m * corner(1),
        m * corner(2),
        m * corner(3),

        // top midpoints
        m * ((corner(4) + corner(5)) * 0.5),
        m * ((corner(5) + corner(7)) * 0.5),
        m * ((corner(7) + corner(6)) * 0.5),
        m * ((corner(4) + corner(6)) * 0.5),
        m * ((corner(4) + corner(7)) * 0.5),

        // bottom midpoints
        m * ((corner(0) + corner(1)) * 0.5),
        m * ((corner(1) + corner(3)) * 0.5),
        m * ((corner(3) + corner(2)) * 0.5),
        m * ((corner(0) + corner(2)) * 0.5),
        m * ((corner(0) + corner(3)) * 0.5)
    };

    // Update the horizon culler.
    _horizonCuller.set(_tileKey.profile().srs(), m, _localbbox);

#ifdef RENDER_TILE_BBOX
    if (children.size() == 2)
        children.resize(1);

    // Builds a debug boundingbox.
    if (_runtime.compiler)
    {
        auto builder = vsg::Builder::create();
        vsg::StateInfo stateinfo;
        stateinfo.wireframe = true;
        vsg::GeometryInfo geominfo;
        geominfo.set(vsg::box(_localbbox));
        auto debug_node = builder->createBox(geominfo, stateinfo);
        _runtime.compiler()->compile(debug_node);
        addChild(debug_node);
    }
#endif
}

void
SurfaceNode::setElevation(shared_ptr<Image> raster, const dmat4& scaleBias)
{
    _elevationRaster = raster;
    _elevationMatrix = scaleBias;
    _boundsDirty = true;
}
