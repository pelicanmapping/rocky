/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Math.h"
#include <algorithm>
#include <float.h>

using namespace rocky;

//------------------------------------------------------------------------

Box
Box::intersection_with(const Box& rhs) const
{
    if (valid() && !rhs.valid()) return *this;
    if (!valid() && rhs.valid()) return rhs;

    if (contains(rhs)) return rhs;
    if (rhs.contains(*this)) return *this;

    if (!intersects(rhs)) return Box();

    double x0, x1, y0, y1, z0, z1;

    x0 = (xmin > rhs.xmin && xmin < rhs.xmax) ? xmin : rhs.xmin;
    x1 = (xmax > rhs.xmin && xmax < rhs.xmax) ? xmax : rhs.xmax;
    y0 = (ymin > rhs.ymin && ymin < rhs.ymax) ? ymin : rhs.ymin;
    y1 = (ymax > rhs.ymin && ymax < rhs.ymax) ? ymax : rhs.ymax;
    z0 = (zmin > rhs.zmin && zmin < rhs.zmax) ? zmin : rhs.zmin;
    z1 = (zmax > rhs.zmin && zmax < rhs.zmax) ? zmax : rhs.zmax;

    return Box(x0, y0, z0, x1, y1, z1);
}

Box
Box::union_with(const Box& rhs) const
{
    if (valid() && !rhs.valid()) return *this;
    if (!valid() && rhs.valid()) return rhs;

    Box u;
    if (intersects(rhs))
    {
        u.xmin = xmin >= rhs.xmin && xmin <= rhs.xmax ? xmin : rhs.xmin;
        u.xmax = xmax >= rhs.xmin && xmax <= rhs.xmax ? xmax : rhs.xmax;
        u.ymin = ymin >= rhs.ymin && ymin <= rhs.ymax ? ymin : rhs.ymin;
        u.ymax = ymax >= rhs.ymin && ymax <= rhs.ymax ? ymax : rhs.ymax;
        u.zmin = zmin >= rhs.zmin && zmin <= rhs.zmax ? zmin : rhs.zmin;
        u.zmax = zmax >= rhs.zmin && zmax <= rhs.zmax ? zmax : rhs.zmax;
    }
    return u;
}

bool
Box::contains(const Box& rhs) const
{
    return
        valid() && rhs.valid() &&
        xmin <= rhs.xmin && xmax >= rhs.xmax &&
        ymin <= rhs.ymin && ymax >= rhs.ymax;
}

#if 0
namespace
{
    double cross2d(const dvec3& a, const dvec3& b) {
        return a.x()*b.y() - b.x()*a.y();
    }

    // ignores Z.
    bool intersectRaysXY(const dvec3& p0, const dvec3& d0,
                         const dvec3& p1, const dvec3& d1,
                         dvec3& out_p,
                         double&     out_u,
                         double&     out_v)
    {
        double det = cross2d(d0, d1);

        if (equiv(det, (0.0)))
            return false;

        out_u = cross2d(p1-p0, d1) / det;
        out_v = cross2d(p1-p0, d0) / det;

        out_p = p0 + d0 * out_u;

        return true;

        //static const double epsilon = 0.001;

        //double det = d0.y()*d1.x() - d0.x()*d1.y();
        //if ( equiv(det, 0.0, epsilon) )
        //    return false; // parallel

        //out_u = (d1.x()*(p1.y()-p0.y())+d1.y()*(p0.x()-p1.x()))/det;
        //out_v = (d0.x()*(p1.y()-p0.y())+d0.y()*(p0.x()-p1.x()))/det;
        //out_p = p0 + d0*out_u;
        //return true;
    }
}
#endif

#if 0
bool
Line2d::intersect( const Line2d& rhs, osg::Vec4d& out ) const
{
    double u, v;
    dvec3 temp;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, (rhs._b-rhs._a), temp, u, v);
    out.set( temp.x(), temp.y(), temp.z(), 1.0 );
    return ok;
}

bool
Line2d::intersect( const Line2d& rhs, dvec3& out ) const
{
    double u, v;
    return intersectRaysXY(_a, (_b-_a), rhs._a, (rhs._b-rhs._a), out, u, v);
}

bool
Line2d::intersect( const Segment2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, (rhs._b-rhs._a), out, u, v);
    return ok && v >= 0.0 && v <= 1.0;
}

bool
Line2d::intersect( const Ray2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, rhs._dv, out, u, v);
    return ok && v >= 0.0;
}

bool
Line2d::intersect( const Line2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Line2d::intersect( const Segment2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Line2d::intersect( const Ray2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Line2d::isPointOnLeft( const dvec3& p ) const
{
    return ((_b-_a) ^ (p-_a)).z() >= 0.0;
}

bool
Line2d::isPointOnLeft( const dvec2& p ) const
{
    return ((_b-_a) ^ (dvec3(p.x(),p.y(),0)-_a)).z() >= 0.0;
}

//--------------------------------------------------------------------------

bool
Ray2d::intersect( const Line2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, _dv, rhs._a, (rhs._b-rhs._a), out, u, v);
    return ok && u >= 0.0;
}

bool
Ray2d::intersect( const Segment2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, _dv, rhs._a, (rhs._b-rhs._a), out, u, v);
    return ok && u >= 0.0 && v >= 0.0 && v <= 1.0;
}

bool
Ray2d::intersect( const Ray2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, _dv, rhs._a, rhs._dv, out, u, v);
    return ok && u >= 0.0 && v >= 0.0;
}

bool
Ray2d::intersect( const Line2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Ray2d::intersect( const Segment2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Ray2d::intersect( const Ray2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Ray2d::isPointOnLeft( const dvec3& p ) const
{
    return (_dv ^ (p-_a)).z() >= 0.0;
}

bool
Ray2d::isPointOnLeft( const dvec2& p ) const
{
    return (_dv ^ (dvec3(p.x(),p.y(),0)-_a)).z() >= 0.0;
}

double
Ray2d::angle(const Segment2d& rhs) const
{
    dvec2 v0( _dv.x(), _dv.y() );
    v0.normalize();
    dvec2 v1(rhs._b.x()-rhs._a.x(), rhs._b.y()-rhs._a.y()); 
    v1.normalize();
    return acos( v0 * v1 );
}

//--------------------------------------------------------------------------

bool
Segment2d::intersect( const Line2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, (rhs._b-rhs._a), out, u, v);
    return ok && u >= 0.0 && u <= 1.0;
}

bool
Segment2d::intersect( const Segment2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, (rhs._b-rhs._a), out, u, v);
    return ok && u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0;
}

bool
Segment2d::intersect( const Ray2d& rhs, dvec3& out ) const
{
    double u, v;
    bool ok = intersectRaysXY(_a, (_b-_a), rhs._a, rhs._dv, out, u, v);
    return ok && u >= 0.0 && u <= 1.0 && v >= 0.0;
}

bool
Segment2d::intersect( const Line2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Segment2d::intersect( const Segment2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Segment2d::intersect( const Ray2d& rhs, dvec2& out ) const
{
    dvec3 out3;
    bool ok = intersect( rhs, out3 );
    out.set(out3.x(), out3.y());
    return ok;
}

bool
Segment2d::isPointOnLeft( const dvec3& p ) const
{
    return ((_b-_a) ^ (p-_a)).z() >= 0.0;
}

bool
Segment2d::isPointOnLeft( const dvec2& p ) const
{
    return ((_b-_a) ^ (dvec3(p.x(),p.y(),0)-_a)).z() >= 0.0;
}

double
Segment2d::angle(const Segment2d& rhs) const
{
    dvec2 v0(_b.x()-_a.x(), _b.y()-_a.y());
    v0.normalize();
    dvec2 v1(rhs._b.x()-rhs._a.x(), rhs._b.y()-rhs._a.y());
    v1.normalize();
    return acos( v0 * v1 );
}

double
Segment2d::squaredDistanceTo(const dvec3& p) const
{
    typedef dvec3 vec3;

    vec3 n = _b - _a;
    vec3 pa = _a - p;

    double c = n * pa;

    if (c > 0.0)
        return (pa*pa);

    vec3 bp = p - _b;

    if ((n*bp) > 0.0)
        return (bp*bp);

    vec3 e = pa - n * (c/(n*n));
    return (e*e);

    //dvec3 n = _b - _a;
    //dvec3 pa = _a - p;
    //double q = (pa*n)/(n*n);
    //dvec3 c = n * q;
    //dvec3 d = pa - c;
    //return sqrt(d*d);

    //double num = (p.x()-_a.x())*(_b.x()-_a.x()) + (p.y()-_a.y())*(_b.y()-_a.y());
    //double demon = (_b-_a).length2();
    //double u = num/demon;
    //if (u < 0.0) return (_a-p).length();
    //if (u > 1.0) return (_a-p).length();
    //dvec3 c(_a.x() + u*(_b.x()-_a.x()), _a.y() + u*(_b.y()-_a.y()), 0);
    //return (c-p).length();
}

dvec3
Segment2d::closestPointTo(const dvec3& p) const
{
    typedef dvec3 vec;
    vec qp = _b-_a;
    vec xp = p-_a;
    double u = (xp*qp)/(qp*qp);
    if (u < 0.0) return _a;
    if (u > 1.0) return _b;
    return _a+qp*u;
}
//
//
//
//    double num = (p.x()-_a.x())*(_b.x()-_a.x()) + (p.y()-_a.y())*(_b.y()-_a.y());
//    double demon = (_b-_a).length2();
//    double u = num/demon;
//    if (u < 0.0) return _a;
//    if (u > 1.0) return _b;
//    return dvec3 (_a.x() + u*(_b.x()-_a.x()), _a.y() + u*(_b.y()-_a.y()), 0);
//}

//--------------------------------------------------------------------------

Segment3d
Segment2d::unrotateTo3D(const osg::Quat& q) const
{
    osg::Quat qi = q.inverse();
    return Segment3d( qi*_a, qi*_b );
}

//--------------------------------------------------------------------------

bool
Triangle2d::contains(const dvec3& p) const
{
    if ( !Line2d(_a, _b).isPointOnLeft(p) ) return false;
    if ( !Line2d(_b, _c).isPointOnLeft(p) ) return false;
    if ( !Line2d(_c, _a).isPointOnLeft(p) ) return false;
    return true;
}

namespace
{
    bool vecLessX(const dvec3& lhs, const dvec3& rhs)
    {
        return lhs.x() < rhs.x();
    }

    bool vecLessY(const dvec3& lhs, const dvec3& rhs)
    {
        return lhs.y() < rhs.y();
    }
}

osg::BoundingBoxd
rocky::polygonBBox2d(const dvec3Array& points)
{
    osg::BoundingBoxd result(DBL_MAX, DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX, -DBL_MAX);
    for (dvec3Array::const_iterator itr = points.begin(); itr != points.end(); ++itr)
    {
        result.xMin() = std::min(result.xMin(), itr->x());
        result.xMax() = std::max(result.xMax(), itr->x());
        result.yMin() = std::min(result.yMin(), itr->y());
        result.yMax() = std::max(result.yMax(), itr->y());
    }
    return result;
}

// Winding number test; see http://geomalgorithms.com/a03-_inclusion.html
bool
rocky::pointInPoly2d(const dvec3& pt, const Polygon& polyPoints, double tolerance)
{
    int windingNum = 0;

    for (dvec3Array::const_iterator itr = polyPoints.begin(); itr != polyPoints.end(); ++itr)
    {
        Segment2d seg = (itr + 1 == polyPoints.end()
                         ? Segment2d(*itr, polyPoints.front())
                         : Segment2d(*itr, *(itr + 1)));
        // if the segment is horizontal, then the "is left" test
        // isn't meaningful. We count the point as in if it's on or
        // to the left of the segment.


        if (seg._a.y() == seg._b.y() && fabs(pt.y() - seg._a.y()) <= tolerance)
        {
            if (pt.x() < seg._a.x() || pt.x() < seg._b.x())
            {
                windingNum++;
            }
        }
        else if (seg._a.y() <= pt.y())
        {
            if (seg._b.y() > pt.y())
            {
                double dist = seg.leftDistanceXY(pt);
                if (dist > -tolerance)
                {
                    windingNum++;
                }
            }
        }
        else if (seg._b.y() <= pt.y())
        {
                double dist = seg.leftDistanceXY(pt);
                if (dist < tolerance)
                {
                    windingNum--;
                }
        }
    }
    return windingNum != 0;
}

bool
rocky::pointInPoly2d(const dvec3& pt, const osg::Geometry* polyPoints, float tolerance)
{
    const osg::Vec3Array *vertices= dynamic_cast<const osg::Vec3Array*>(polyPoints->getVertexArray());
    if (!vertices)
        return false;
    int windingNum = 0;
    for (unsigned int ipr=0; ipr< polyPoints->getNumPrimitiveSets(); ipr++)
    {
        const osg::PrimitiveSet* prset = polyPoints->getPrimitiveSet(ipr);
        if (prset->getMode()==osg::PrimitiveSet::LINE_LOOP)
        {
            const osg::Vec3 prev=(*vertices)[prset->index(prset->getNumIndices()-1)];
            for (unsigned int i=0; i<prset->getNumIndices(); i++)
            {
                Segment2d seg = ((i == 0) ? Segment2d(prev, (*vertices)[prset->index(0)])
                                 : Segment2d((*vertices)[prset->index(i - 1)], (*vertices)[prset->index(i)]));
                if (seg._a.y() == seg._b.y() && fabs(pt.y() - seg._a.y()) <= tolerance)
                {
                    if (pt.x() < seg._a.x() || pt.x() < seg._b.x())
                    {
                        windingNum++;
                    }
                }
                else if (seg._a.y() <= pt.y())
                {
                    if (seg._b.y() > pt.y())
                    {
                        double dist = seg.leftDistanceXY(pt);
                        if (dist > -tolerance)
                        {
                            windingNum++;
                        }
                    }
                }
                else if (seg._b.y() <= pt.y())
                {
                    double dist = seg.leftDistanceXY(pt);
                    if (dist < tolerance)
                    {
                        windingNum--;
                    }
                }
            }
        }
    }
    return windingNum != 0;
}
#endif

#if 0
bool
ProjectionMatrix::isOrtho(const dmat4& m)
{
    return !m.isIdentity() && m(3, 3) > 0.0;
}

bool
ProjectionMatrix::isPerspective(const dmat4& m)
{
    return m(3, 3) == 0.0; // can't be identity if this is true
}

ProjectionMatrix::Type
ProjectionMatrix::getType(const dmat4& m)
{
    if (m.isIdentity())
    {
        return UNKNOWN;
    }
    else if (m(2, 2) > 0.0)
    {
        return REVERSE_Z;
    }
    else
    {
        return STANDARD;
    }
}

void
ProjectionMatrix::setPerspective(
    dmat4& m,
    double vfov, double ar, double N, double F,
    ProjectionMatrix::Type type)
{
    if (type == UNKNOWN)
    {
        type = getType(m);
    }

    if (type == REVERSE_Z)
    {
        double k = tan(deg2rad(vfov*0.5));
        double
            L = k * ar * N,
            R = -L,
            T = k * N,
            B = -T;

        m.set(
            2 * N / (R - L), 0, 0, 0,
            0, 2 * N / (T - B), 0, 0,
            (R + L) / (R - L), (T + B) / (T - B), N / (F - N), -1,
            0, 0, F*N / (F - N), 0);
    }
    else
    {
        m.makePerspective(vfov, ar, N, F);
    }
}

bool
ProjectionMatrix::getPerspective(
    const dmat4& m,
    double& vfov, double& ar, double& N, double& F)
{
    if (!isPerspective(m))
        return false;

    if (getType(m) == REVERSE_Z)
    {
        double L, R, B, T, N, F;
        if (getPerspective(m, L, R, B, T, N, F))
        {
            vfov = rad2deg((T / N) - atan(B / N));
            ar = (R - L) / (T - B);
        }
        else return false;
    }
    else
    {
        m.getPerspective(vfov, ar, N, F);
    }

    return true;
}

bool
ProjectionMatrix::getPerspective(
    const dmat4& m,
    double& L, double& R, double& B, double& T, double& N, double& F)
{
    if (!isPerspective(m))
        return false;

    if (getType(m) == REVERSE_Z)
    {
        // don't use N and F directly in case they refer to the same var
        double temp_near = m(3, 2) / (1.0 + m(2, 2));
        double temp_far = m(3, 2) / m(2, 2);

        L = temp_near * (m(2, 0) - 1.0) / m(0, 0);
        R = temp_near * (1.0 + m(2, 0)) / m(0, 0);
        T = temp_near * (1.0 + m(2, 1)) / m(1, 1);
        B = temp_near * (m(2, 1) - 1.0) / m(1, 1);
        N = temp_near;
        F = temp_far;
    }
    else
    {
        m.getFrustum(L, R, B, T, N, F);
    }

    return true;
}

void
ProjectionMatrix::setOrtho(
    dmat4& m,
    double L, double R, double B, double T, double N, double F,
    Type type)
{
    if (type == UNKNOWN)
    {
        type = getType(m);
    }

    if (type == REVERSE_Z)
    {
        double tx = -(R + L) / (R - L);
        double ty = -(T + B) / (T - B);
        double tz = F / (F - N); // <- reverse Z

        m.set(
            2.0 / (R - L), 0.0, 0.0, 0.0,
            0.0, 2.0 / (T - B), 0.0, 0.0,
            0.0, 0.0, 1. / (F - N), 0.0,
            tx, ty, tz, 1.0);
    }
    else
    {
        m.makeOrtho(L, R, B, T, N, F);
    }
}

bool 
ProjectionMatrix::getOrtho(
    const dmat4& m,
    double& L, double& R, double& B, double& T, double& N, double& F)
{
    if (!isOrtho(m))
        return false;

    if (getType(m) == REVERSE_Z)
    {
        double c = 1.0 / m(2, 2);
        F = m(3, 2) * c;
        N = F - c;

        //double i = mat(2, 2), j = mat(3, 2);
        //N = (1.0 + j) / (-i * j);
        //F = N * (1.0 - j) / (1.0 + j);

        L = -(1.0 + m(3, 0)) / m(0, 0);
        R = (1.0 - m(3, 0)) / m(0, 0);
        B = -(1.0 + m(3, 1)) / m(1, 1);
        T = (1.0 - m(3, 1)) / m(1, 1);

        return true;
    }
    else
    {
        return m.getOrtho(L, R, B, T, N, F);
    }
}
#endif