/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapNode.h"
#include "VSGUtils.h"
#include "json.h"
#include <rocky/Horizon.h>
#include <rocky/vsg/NodeLayer.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef LC
#define LC "[MapNode] "

MapNode::MapNode(VSGContext context)
{
    map = Map::create();

    terrainNode = TerrainNode::create(context);
    this->addChild(terrainNode);

    // default to geodetic:
    profile = Profile("global-geodetic");
}

Result<>
MapNode::from_json(const std::string& JSON, const IOOptions& io)
{
    const auto j = parse_json(JSON);
    if (j.status.failed())
        return j.status.error();

    if (map && j.contains("map"))
    {        
        auto r = map->from_json(j.at("map").dump(), io);
        if (r.failed())
            return r.error();
    }

    if (j.contains("profile"))
    {
        get_to(j, "profile", profile);
    }

    if (terrainNode && j.contains("terrain"))
    {
        auto r = terrainNode->from_json(j.at("terrain").dump(), io);
        if (r.failed())
            return r.error();
    }

    return ResultVoidOK;
}

std::string
MapNode::to_json() const
{
    auto j = json::object();

    if (map)
    {
        j["map"] = json::parse(map->to_json());
    }

    if (terrainNode)
    {
        j["terrain"] = json::parse(terrainNode->to_json());
    }

    if (profile.valid() && profile != Profile("global-geodetic"))
    {
        j["profile"] = profile;
    }

    return j.dump();
}

const TerrainSettings&
MapNode::terrainSettings() const
{
    return *terrainNode.get();
}

TerrainSettings&
MapNode::terrainSettings()
{
    return *terrainNode.get();
}

const SRS&
MapNode::srs() const
{
    if (profile.srs().isGeodetic() || profile.srs().isQSC())
        return profile.srs().geocentricSRS();
    else
        return profile.srs();
}

bool
MapNode::update(VSGContext context)
{
    //ROCKY_HARD_ASSERT_STATUS(context.status());
    ROCKY_HARD_ASSERT(map != nullptr && terrainNode != nullptr);

    bool changes = false;

    if (terrainNode->map == nullptr || terrainNode->profile != profile)
    {
        auto st = terrainNode->setMap(map, profile, srs(), context);

        if (st.failed())
        {
            Log()->warn(st.error().message);
        }
    }

    // on our first update, open any layers that are marked to automatic opening.
    if (!_openedLayers)
    {
        auto r = map->openAllLayers(context->io);
        if (r.failed())
        {
            Log()->warn("Failed to open at least one layer... " + r.error().message);
        }

        _openedLayers = true;
    }

    return terrainNode->update(context);
}

void
MapNode::traverse(vsg::RecordTraversal& record) const
{
    auto viewID = record.getCommandBuffer()->viewID;

    auto& horizon = _horizon[viewID];

    if (srs().isGeocentric())
    {
        if (!horizon)
        {
            horizon.setEllipsoid(srs().ellipsoid());
        }

        auto eye = vsg::inverse(record.getState()->modelviewMatrixStack.top()) * vsg::dvec3(0, 0, 0);
        bool is_ortho = record.getState()->projectionMatrixStack.top()(3, 3) != 0.0;
        horizon.setEye(to_glm(eye), is_ortho);

        record.setValue("rocky.horizon", &_horizon);
    }

    record.setValue("rocky.worldsrs", srs());

    record.setObject("rocky.terraintilehost", terrainNode);

    Inherit::traverse(record);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(record);
            }
        });
}

void
MapNode::traverse(vsg::Visitor& visitor)
{
    visitor.setValue("rocky.worldsrs", srs());

    Inherit::traverse(visitor);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(visitor);
            }
        });
}

void
MapNode::traverse(vsg::ConstVisitor& visitor) const
{
    visitor.setValue("rocky.worldsrs", srs());

    Inherit::traverse(visitor);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(visitor);
            }
        });
}

vsg::ref_ptr<MapNode>
MapNode::create(VSGContext context)
{
    //ROCKY_SOFT_ASSERT_AND_RETURN(context, {}, "ILLEGAL: null context");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer(), {}, "ILLEGAL: context does not contain a viewer");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer()->updateOperations, {}, "ILLEGAL: viewer does not contain update operations");

    auto mapNode = vsg::ref_ptr<MapNode>(new MapNode(context));

    if (context && context->viewer() && context->viewer()->updateOperations)
    {
        auto update = [mapNode, context]()
            {
                context->update();
                mapNode->update(context);
            };

        context->viewer()->updateOperations->add(LambdaOperation::create(update), vsg::UpdateOperations::ALL_FRAMES);
    }

    return mapNode;
}

Viewpoint
MapNode::createViewpoint(const std::vector<GeoPoint>& points, vsg::Camera* camera) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(!points.empty(), {});
    ROCKY_SOFT_ASSERT_AND_RETURN(camera, {});

    auto projMatrix = vsg::clone(camera->projectionMatrix);
    auto viewMatrix = vsg::clone(camera->viewMatrix);
    
    // perspective only for now.
    bool isPerspective = projMatrix->is_compatible(typeid(vsg::Perspective));

    // Convert the point set to world space:
    std::vector<glm::dvec3> world(points.size());

    // Collect the extent so we can calculate the centroid.
    GeoExtent extent(profile.srs());

    for (int i = 0; i < points.size(); ++i)
    {
        GeoPoint p = points[i];

        // transform to the map's srs and then to world coords.
        p.transformInPlace(profile.srs());        
        world[i] = p.transform(srs());

        extent.expandToInclude(p.x, p.y);
    }

    if (extent.area() == 0.0)
    {
        extent.expand(0.001, 0.001);
    }

    double eyeDist = 1.0;
    double fovy_deg = 45.0, ar = 1.0;
    double znear = 1.0, zfar = 1000.0;

    // Calculate the centroid, which will become the focal point of the view:
    auto centroidWorld = extent.centroid().transform(srs());

    if (isPerspective)
    {
        // For a perspective matrix, rewrite the projection matrix so 
        // the far plane is the radius of the ellipsoid. We do this so
        // we can project our control points onto a common plane.
        auto persp = projMatrix->cast<vsg::Perspective>();

        znear = persp->nearDistance;
        zfar = persp->farDistance;
        fovy_deg = persp->fieldOfViewY;
        ar = persp->aspectRatio;

        znear = 1.0;

        if (profile.srs().isGeodetic())
        {
            auto C = centroidWorld;
            C = glm::normalize(C);
            C.z = fabs(C.z);
            double t = glm::dot(C, glm::dvec3(0, 0, 1)); // dot product
            zfar = glm::mix(profile.srs().ellipsoid().semiMajorAxis(), profile.srs().ellipsoid().semiMinorAxis(), t);
            eyeDist = zfar * 2.0;
        }
        else
        {
            vsg::LookAt lookAt;
            lookAt.set(camera->viewMatrix->transform());
            eyeDist = vsg::length(lookAt.eye);
            zfar = eyeDist;
        }

        persp->nearDistance = znear;
        persp->farDistance = zfar;
    }

#if 0
    else // isOrtho
    {
        fovy_deg = _vfov;
        double L, R, B, T, N, F;
        ProjectionMatrix::getOrtho(projMatrix, L, R, B, T, N, F);
        ar = (R - L) / (T - B);

        if (_mapSRS->isGeographic())
        {
            osg::Vec3d C = centroid;
            C.normalize();
            C.z() = fabs(C.z());
            double t = C * osg::Vec3d(0, 0, 1); // dot product

            zfar = mix(_mapSRS->getEllipsoid().getRadiusEquator(),
                _mapSRS->getEllipsoid().getRadiusPolar(),
                t);
            eyeDist = zfar * 2.0;
        }
        else
        {
            osg::Vec3d eye, center, up2;
            viewMatrix.getLookAt(eye, center, up2);
            eyeDist = eye.length();
            zfar = eyeDist;
        }
    }
#endif

    // Set up a new view matrix to look down on that centroid:
    glm::dvec3 lookAt, up;
    glm::dvec3 lookFrom = centroidWorld;

    if (profile.srs().isGeodetic())
    {
        lookFrom = glm::normalize(lookFrom);
        lookFrom *= eyeDist;
        lookAt = centroidWorld;
        up = glm::dvec3(0, 0, 1);
    }
    else
    {
        lookFrom.z = eyeDist;
        lookAt = glm::dvec3(lookFrom.x, lookFrom.y, 0);
        up = glm::dvec3(0, 1, 0);
    }

    viewMatrix = vsg::LookAt::create(to_vsg(lookFrom), to_vsg(lookAt), to_vsg(up));
    auto view_mat = to_glm(viewMatrix->transform());

    double Mx = -DBL_MAX, My = -DBL_MAX;
    std::vector<glm::dvec3> view(world.size());
    for (int i = 0; i < world.size(); ++i)
    {
        // Transform into view space (camera-relative):
        view[i] = view_mat * world[i];
        Mx = std::max(Mx, std::abs(view[i].x));
        My = std::max(My, std::abs(view[i].y));
    }

    // Apply the edge buffer:
    //Mx += _buffer_m;
    //My += _buffer_m;

    // Calculate optimal new Z (distance from view plane)
    double half_fovy_rad = glm::radians(fovy_deg) * 0.5;
    double half_fovx_rad = half_fovy_rad * ar;
    double Zx = Mx / tan(half_fovx_rad);
    double Zy = My / tan(half_fovy_rad);
    double Zbest = std::max(Zx, Zy);

    // Convert to a geopoint
    GeoPoint FP(srs(), centroidWorld);
    FP.transformInPlace(profile.srs());
    
    Viewpoint out;
    out.point = FP;
    out.pitch = Angle(-90, Units::DEGREES);
    out.range = Distance(Zbest, Units::METERS);

    return out;
}