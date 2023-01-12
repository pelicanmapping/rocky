/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Horizon.h"

using namespace rocky;

Horizon::Horizon() :
    _valid(false),
    _VCmag(0), _VCmag2(0), _VHmag2(0), _coneCos(0), _coneTan(0), _minVCmag(0), _minHAE(0)
{
    setEllipsoid(Ellipsoid());
}

Horizon::Horizon(const Ellipsoid& e) :
    _valid(false),
    _VCmag(0), _VCmag2(0), _VHmag2(0), _coneCos(0), _coneTan(0), _minVCmag(0), _minHAE(0)
{
    setEllipsoid(e);
}

void
Horizon::setEllipsoid(const Ellipsoid& em)
{
    _em = em;

    _scaleInv = dvec3(
        em.semiMajorAxis(),
        em.semiMajorAxis(),
        em.semiMinorAxis());

    _scale = dvec3(
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMinorAxis());

    _minHAE = 500.0;
    _minVCmag = 1.0 + (_scale*_minHAE).length();

    // just so we don't have gargabe values
    setEye(dvec3(1e7, 0, 0));

    _valid = true;
}

void
Horizon::setMinHAE(double value)
{
    _minHAE = value;
    _minVCmag = 1.0 + (_scale*_minHAE).length();
}

bool
Horizon::setEye(const dvec3& eye)
{
    if (eye == _eye)
        return false;

    _eye = eye;
    _eyeUnit = glm::normalize(eye);

    _VC = dvec3(
        -_eye.x * _scale.x,
        -_eye.y * _scale.y,
        -_eye.z * _scale.z);  // viewer->center (scaled)
    _VCmag = std::max((double)_VC.length(), _minVCmag);      // clamped to the min HAE
    _VCmag2 = _VCmag * _VCmag;
    _VHmag2 = _VCmag2 - 1.0;  // viewer->horizon line (scaled)

    double VPmag = _VCmag - 1.0 / _VCmag; // viewer->horizon plane dist (scaled)
    double VHmag = sqrtf(_VHmag2);

    _coneCos = VPmag / VHmag; // cos of half-angle of horizon cone
    _coneTan = tan(acos(_coneCos));

    return true;
}

double
Horizon::getRadius() const
{
    return dvec3(_eyeUnit.x*_scaleInv.x, _eyeUnit.y*_scaleInv.y, _eyeUnit.z*_scaleInv.z).length();
}

bool
Horizon::isVisible(
    const dvec3& target,
    double radius) const
{
    if (_valid == false || radius >= _scaleInv.x || radius >= _scaleInv.y || radius >= _scaleInv.z)
        return true;

    // First check the object against the horizon plane, a plane that intersects the
    // ellipsoid, whose normal is the vector from the eyepoint to the center of the
    // ellipsoid.
    // ref: https://cesiumjs.org/2013/04/25/Horizon-culling/

    // Viewer-to-target vector
    dvec3 VT;

    // move the target closer to the horizon plane by "radius".
    VT = (target + _eyeUnit * radius) - _eye;

    // transform into unit space:
    VT = dvec3(VT.x*_scale.x, VT.y*_scale.y, VT.z*_scale.z);

    // If the target is above the eye, it's visible
    double VTdotVC = glm::dot(VT, _VC); // VT * _VC;
    if (VTdotVC <= 0.0)
    {
        return true;
    }

    // If the eye is below the ellipsoid, but the target is below the eye
    // (since the above test failed) the target is occluded.
    // NOTE: it might be better instead to check for a maximum distance from
    // the eyepoint instead.
    if (_VCmag < 0.0)
    {
        return false;
    }

    // Now we know that the eye is above the ellipsoid, so there is a valid horizon plane.
    // If the point is in front of that horizon plane, it's visible and we're done
    if (VTdotVC <= _VHmag2)
    {
        return true;
    }

    // The sphere is completely behind the horizon plane. So now intersect the
    // sphere with the horizon cone, a cone eminating from the eyepoint along the
    // eye->center vetor. If the sphere is entirely within the cone, it is occluded
    // by the spheroid (not ellipsoid, sorry)
    // ref: http://www.cbloom.com/3d/techdocs/culling.txt
    VT = target - _eye;

    double a = glm::dot(VT, -_eyeUnit); // VT * -_eyeUnit;
    double b = a * _coneTan;
    double c = sqrt(glm::dot(VT, VT) - a * a);
    double d = c - b;
    double e = d * _coneCos;

    if (e > -radius)
    {
        // sphere is at least partially outside the cone (visible)
        return true;
    }

    // occluded.
    return false;
}


bool
Horizon::isVisible(
    const dvec3& eye,
    const dvec3& target,
    double radius) const
{
    if (_valid == false || radius >= _scaleInv.x || radius >= _scaleInv.y || radius >= _scaleInv.z)
        return true;

    optional<dvec3> eyeUnit;

    dvec3 VC = dvec3(
        -_eye.x*_scale.x,
        -_eye.y*_scale.y,
        -_eye.z*_scale.z); // osg::componentMultiply(-eye, _scale);  // viewer->center (scaled)

    // First check the object against the horizon plane, a plane that intersects the
    // ellipsoid, whose normal is the vector from the eyepoint to the center of the
    // ellipsoid.
    // ref: https://cesiumjs.org/2013/04/25/Horizon-culling/

    // Viewer-to-target
    dvec3 delta(0, 0, 0);
    if (radius > 0.0)
    {
        eyeUnit = glm::normalize(eye);
        //eyeUnit->normalize();
        delta = eyeUnit.value() * radius;
    }
    dvec3 VT(target + delta - eye);

    // transform into unit space:
    VT = dvec3(VT.x*_scale.x, VT.y*_scale.y, VT.z*_scale.z);

    // If the target is above the eye, it's visible
    double VTdotVC = glm::dot(VT, VC); // VT * VC;
    if (VTdotVC <= 0.0)
    {
        return true;
    }

    // If the eye is below the ellipsoid, but the target is below the eye
    // (since the above test failed) the target is occluded.
    // NOTE: it might be better instead to check for a maximum distance from
    // the eyepoint instead.
    double VCmag = std::max((double)VC.length(), _minVCmag);      // clamped to the min HAE
    if (VCmag < 0.0)
    {
        return false;
    }

    // Now we know that the eye is above the ellipsoid, so there is a valid horizon plane.
    // If the point is in front of that horizon plane, it's visible and we're done
    double VHmag2 = VCmag * VCmag - 1.0;  // viewer->horizon line (scaled)
    if (VTdotVC <= VHmag2)
    {
        return true;
    }

    // The sphere is completely behind the horizon plane. So now intersect the
    // sphere with the horizon cone, a cone eminating from the eyepoint along the
    // eye->center vetor. If the sphere is entirely within the cone, it is occluded
    // by the spheroid (not ellipsoid, sorry)
    // ref: http://www.cbloom.com/3d/techdocs/culling.txt
    VT = target - eye;

    double VPmag = VCmag - 1.0 / VCmag; // viewer->horizon plane dist (scaled)
    double VHmag = sqrtf(VHmag2);
    double coneCos = VPmag / VHmag; // cos of half-angle of horizon cone
    double coneTan = tan(acos(coneCos));

    if (!eyeUnit.isSet()) {
        eyeUnit = glm::normalize(eye);
    }
    double a = glm::dot(VT, -eyeUnit.value());
    double b = a * coneTan;
    double c = sqrt(glm::dot(VT, VT) - a * a);
    double d = c - b;
    double e = d * coneCos;

    if (e > -radius)
    {
        // sphere is at least partially outside the cone (visible)
        return true;
    }

    // occluded.
    return false;
}

#if 0
bool
Horizon::getPlane(osg::Plane& out_plane) const
{
    // calculate scaled distance from center to viewer:
    if (_valid == false || _VCmag2 == 0.0)
        return false;

    double PCmag;
    if (_VCmag > 0.0)
    {
        // eyepoint is above ellipsoid? Calculate scaled distance from center to horizon plane
        PCmag = 1.0 / _VCmag;
    }
    else
    {
        // eyepoint is below the ellipsoid? plane passes through the eyepoint.
        PCmag = _VCmag;
    }

    osg::Vec3d pcWorld = osg::componentMultiply(_eyeUnit*PCmag, _scaleInv);
    double dist = pcWorld.length();

    // compute a new clip plane:
    out_plane.set(_eyeUnit, -dist);
    return true;
}
#endif

double
Horizon::getDistanceToVisibleHorizon() const
{
    double eyeLen = _eye.length();

    dvec3 geodetic;

    geodetic = _em.geocentricToGeodetic(_eye);
    double hasl = geodetic.z;

    // limit it:
    hasl = std::max(hasl, 100.0);

    double radius = eyeLen - hasl;
    return sqrt(2.0*radius*hasl + hasl * hasl);
}

#if 0
//........................................................................


HorizonCullCallback::HorizonCullCallback() :
    _enabled(true),
    _centerOnly(false),
    _customEllipsoidSet(false)
{
    //nop
}

void
HorizonCullCallback::setEllipsoid(const Ellipsoid& em)
{
    _customEllipsoid.setSemiMajorAxis(em.semiMajorAxis());
    _customEllipsoid.setSemiMinorAxis(em.semiMinorAxis());
    _customEllipsoidSet = true;
}

bool
HorizonCullCallback::isVisible(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!node)
        return false;

    osg::ref_ptr<Horizon> horizon;
    ObjectStorage::get(nv, horizon);

    if (_customEllipsoidSet)
    {
        osg::Vec3d eye;
        if (horizon.valid())
            eye = horizon->getEye();
        else
            eye = osg::Vec3d(0, 0, 0) * Culling::asCullVisitor(nv)->getCurrentCamera()->getInverseViewMatrix();

        horizon = new Horizon(_customEllipsoid);
        horizon->setEye(eye);
    }

    // If we fetched the Horizon from the nodevisitor...
    if (horizon.valid())
    {
        // pop the last node in the path (which is the node this callback is on)
        // to prevent double-transforming the bounding sphere's center point
        osg::NodePath np = nv->getNodePath();
        if (!np.empty() && np.back() == node)
            np.pop_back();

        osg::Matrix local2world = osg::computeLocalToWorld(np);

        const osg::BoundingSphere& bs = node->getBound();
        double radius = _centerOnly ? 0.0 : bs.radius();
        return horizon->isVisible(bs.center()*local2world, radius);
    }

    // If the user forgot to install a horizon at all...
    else
    {
        // No horizon data... just assume visibility
        //OE_WARN << LC << "No horizon info installed in callback\n";
        return true;
    }
}

void
HorizonCullCallback::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (_enabled)
    {
        if (_proxy.valid())
        {
            osg::ref_ptr<osg::Node> proxy;
            if (_proxy.lock(proxy))
            {
                if (isVisible(proxy.get(), nv))
                    traverse(node, nv);
                return;
            }
        }

        if (isVisible(node, nv))
        {
            traverse(node, nv);
        }
    }

    else
    {
        traverse(node, nv);
    }
}




HorizonNode::HorizonNode()
{
    const float r = 25.0f;
    //osg::DrawElements* de = new osg::DrawElementsUByte(GL_QUADS);
    //de->addElement(0);
    //de->addElement(1);

    osg::Vec3Array* verts = new osg::Vec3Array();
    for (unsigned x = 0; x <= (unsigned)r; ++x) {
        verts->push_back(osg::Vec3(-0.5f + float(x) / r, -0.5f, 0.0f));
        verts->push_back(osg::Vec3(-0.5f + float(x) / r, 0.5f, 0.0f));
    }

    //de->addElement(verts->size()-1);
    //de->addElement(verts->size()-2);

    for (unsigned y = 0; y <= (unsigned)r; ++y) {
        verts->push_back(osg::Vec3(-0.5f, -0.5f + float(y) / r, 0.0f));
        verts->push_back(osg::Vec3(0.5f, -0.5f + float(y) / r, 0.0f));
    }

    osg::Vec4Array* colors = new osg::Vec4Array(osg::Array::BIND_OVERALL);
    colors->push_back(osg::Vec4(1, 0, 0, 0.5f));

    osg::Geometry* geom = new osg::Geometry();
    geom->setName(typeid(*this).name());
    geom->setUseVertexBufferObjects(true);

    geom->setVertexArray(verts);
    geom->setColorArray(colors);
    geom->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, verts->size()));

    //geom->addPrimitiveSet(de);

    osg::Geode* geode = new osg::Geode();
    geode->addDrawable(geom);
    geom->getOrCreateStateSet()->setMode(GL_BLEND, 1);
    geom->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    this->addChild(geode);

    setMatrix(osg::Matrix::scale(15e6, 15e6, 15e6));
}

void
HorizonNode::traverse(osg::NodeVisitor& nv)
{
    bool temp;
    bool isSpy = nv.getUserValue("osgEarth.Spy", temp);

    if (nv.getVisitorType() == nv.CULL_VISITOR)
    {
        if (!isSpy)
        {
            //Horizon* h = Horizon::get(nv);
            osg::ref_ptr<Horizon> h = new Horizon();

            osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(&nv);

            osg::Vec3d eye = osg::Vec3d(0, 0, 0) * cv->getCurrentCamera()->getInverseViewMatrix();
            h->setEye(eye);

            osg::Plane plane;
            if (h->getPlane(plane))
            {
                osg::Quat q;
                q.makeRotate(osg::Plane::Vec3_type(0, 0, 1), plane.getNormal());

                double dist = plane.distance(osg::Vec3d(0, 0, 0));

                osg::Matrix m;
                m.preMultRotate(q);
                m.preMultTranslate(osg::Vec3d(0, 0, -dist));
                m.preMultScale(osg::Vec3d(15e6, 15e6, 15e6));

                setMatrix(m);
            }
        }
        else
        {
            osg::MatrixTransform::traverse(nv);
        }
    }

    else
    {
        osg::MatrixTransform::traverse(nv);
    }
}
#endif
