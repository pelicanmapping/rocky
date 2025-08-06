/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SurfaceNode.h"
#include "GeometryPool.h"
#include <rocky/vsg/VSGUtils.h>
#include <rocky/Heightfield.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[SurfaceNode] "


SurfaceNode::SurfaceNode(const TileKey& tilekey, const SRS& worldSRS) :
    _tilekey(tilekey)
{
    // Establish a local reference frame for the tile:
    GeoPoint centroid = tilekey.extent().centroid().transform(worldSRS);

    glm::dmat4 local2world = worldSRS.topocentricToWorldMatrix(centroid);

    this->matrix = to_vsg(local2world);
}

void
SurfaceNode::setElevation(Image::Ptr raster, const glm::fmat4& scaleBias)
{
    _elevationRaster = raster;
    _elevationMatrix = scaleBias;
    _boundsDirty = true;
    recomputeBound();
}

#define corner(N) vsg::dvec3( \
    (N & 0x1) ? localbbox.max.x : localbbox.min.x, \
    (N & 0x2) ? localbbox.max.y : localbbox.min.y, \
    (N & 0x4) ? localbbox.max.z : localbbox.min.z)

const vsg::dsphere&
SurfaceNode::recomputeBound()
{
    // if the bounds are not dirty, do nothing
    if (!_boundsDirty)
        return worldBoundingSphere;
    else
        _boundsDirty = false;

    // start with a null bbox
    localbbox = { };

    if (children.empty())
        return worldBoundingSphere;

    // locate the geometry
    auto geom = static_cast<vsg::Group*>(children.front().get())->children.front()->cast<SharedGeometry>();
    ROCKY_SOFT_ASSERT_AND_RETURN(geom, worldBoundingSphere);

    if (!_proxyVerts)
    {
        _proxyVerts = vsg::clone(geom->verts);
        _proxyGeom = vsg::Geometry::create();
        _proxyGeom->assignArrays(vsg::DataList{ _proxyVerts });
        _proxyGeom->assignIndices(geom->indexArray);
        _proxyGeom->commands = geom->commands;
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

        ROCKY_SOFT_ASSERT_AND_RETURN(!equiv(scaleU, 0.0) && !equiv(scaleV, 0.0), worldBoundingSphere);

        for (int i = 0; i < geom->verts->size(); ++i)
        {
            if (((int)geom->uvs->at(i).z & VERTEX_HAS_ELEVATION) == 0)
            {
                float h = heightfield->heightAtUV(
                    clamp(geom->uvs->at(i).x * scaleU + biasU, 0.0, 1.0),
                    clamp(geom->uvs->at(i).y * scaleV + biasV, 0.0, 1.0),
                    Interpolation::Bilinear);

                auto& vert = geom->verts->at(i);
                auto& norm = geom->normals->at(i);
                (*_proxyVerts)[i] = vert + norm * h;
            }
            else
            {
                (*_proxyVerts)[i] = (*geom->verts)[i];
            }
        }
    }

    else
    {
        // no elevation? just copy the verts into the proxy
        std::copy(geom->verts->begin(), geom->verts->end(), _proxyVerts->begin());
    }

    // for debugging only
    //_proxyVerts->dirty();

    // build the bbox around the mesh.
    for (auto& vert : *_proxyVerts)
    {
        localbbox.add(vert);
    }

    auto& m = this->matrix;

    // transform the world space to create the bounding sphere
    vsg::dvec3 center = m * ((localbbox.min + localbbox.max) * 0.5);
    double radius = 0.5 * vsg::length(localbbox.max - localbbox.min);
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

    // finally, calculate a horizon culling point for the tile.
    std::vector<glm::dvec3> world_mesh;
    world_mesh.reserve(_proxyVerts->size());
    for (auto& v : *_proxyVerts) {
        auto world = m * vsg::dvec4(v.x, v.y, v.z, 1.0);
        world_mesh.emplace_back(world.x, world.y, world.z);
    }
    auto& ellipsoid = _tilekey.profile.srs().ellipsoid();
    _horizonCullingPoint = to_vsg(ellipsoid.calculateHorizonPoint(world_mesh));
    _horizonCullingPoint_valid = _horizonCullingPoint != vsg::dvec3(0, 0, 0);


#if 0
    if (children.size() == 2)
        children.resize(1);

    // Builds a debug boundingbox.
    auto builder = vsg::Builder::create();
    vsg::StateInfo stateinfo;
    stateinfo.wireframe = true;
    vsg::GeometryInfo geominfo;
    geominfo.set(vsg::box(localbbox));
    geominfo.color.set(1, 1, 0, 1); // broken.
    auto debug_node = builder->createBox(geominfo, stateinfo);
    addChild(debug_node);
#endif

    return worldBoundingSphere;
}
