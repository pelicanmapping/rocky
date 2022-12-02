#include "GeoExtent.h"
#include "Math.h"

using namespace rocky;

#undef  LC
#define LC "[GeoExtent] "

namespace {
    inline bool is_valid(double n) {
        return
            std::isnan(n) == false &&
            n != DBL_MAX &&
            n != -DBL_MAX;
    }
}

GeoExtent GeoExtent::INVALID = GeoExtent();


GeoExtent::GeoExtent() :
    _srs(0L),
    _west(0.0),
    _width(-1.0),
    _south(0.0),
    _height(-1.0)
{
    //NOP - invalid
}

GeoExtent::GeoExtent(shared_ptr<SRS> srs) :
    _srs(srs),
    _west(0.0),
    _width(-1.0),
    _south(0.0),
    _height(-1.0)
{
    //NOP - invalid
}

GeoExtent::GeoExtent(
    shared_ptr<SRS> srs,
    double west, double south, double east, double north) :

    _srs(srs),
    _west(0.0),
    _width(-1.0),
    _south(0.0),
    _height(-1.0)
{
    set(west, south, east, north);
}


GeoExtent::GeoExtent(shared_ptr<SRS> srs, const Box& bounds) :
    _srs(srs),
    _west(0.0),
    _width(-1.0),
    _south(0.0),
    _height(-1.0)
{
    set(bounds.xmin, bounds.ymin, bounds.xmax, bounds.ymax);
}

GeoExtent::GeoExtent(const GeoExtent& rhs) :
    _srs(rhs._srs),
    _west(rhs._west),
    _width(rhs._width),
    _south(rhs._south),
    _height(rhs._height)
{
    //NOP
}

bool
GeoExtent::isGeographic() const
{
    return _srs && _srs->isGeographic();
}

bool
GeoExtent::isWholeEarth() const
{
    return 
        _srs && 
        _srs->isGeographic() &&
        width() == 360.0 &&
        height() == 180.0;
}

void
GeoExtent::set(double west, double south, double east, double north)
{
    // Validate input.
    if (!is_valid(west) ||
        !is_valid(south) ||
        !is_valid(east) ||
        !is_valid(north) ||
        south > north)
    {
        _west = _south = 0.0;
        _width = _height = -1.0;
        return;
    }

    // In this method, east is always to the east of west!
    // If it appears not to be, that means the extent crosses the antimeridian.
    west = normalizeX(west);
    double width = 0.0;
    double height = 0.0;

    if (isGeographic())
    {
        // ensure east >= west in a geographic frame.
        while (east < west)
            east += 360.0;
    }
    width = std::max(0.0, east - west);

    height = std::max(0.0, north - south);

    setOriginAndSize(west, south, width, height);
}

void
GeoExtent::setOriginAndSize(double west, double south, double width, double height)
{
    _west = west;
    _south = south;
    _width = width;
    _height = height;
    clamp();
}

GeoPoint
GeoExtent::getCentroid() const
{
    if (valid())
    {
        return GeoPoint(
            _srs,
            normalizeX(west() + 0.5*width()),
            south() + 0.5*height(),
            ALTMODE_ABSOLUTE);
    }
    else return GeoPoint::INVALID;
}

bool
GeoExtent::getCentroid(double& out_x, double& out_y) const
{
    GeoPoint p = getCentroid();
    out_x = p.x(), out_y = p.y();
    return p.valid();
}

double
GeoExtent::width(const Units& units) const
{
    if (!valid())
        return 0.0;
    else if (getSRS()->isProjected()) {
        return Units::convert(getSRS()->getUnits(), units, width());
    }
    else {
        Distance d(width(), getSRS()->getUnits());
        double m_per_deg = 2.0 * getSRS()->getEllipsoid().getRadiusEquator() * M_PI / 360.0;
        double d0 = m_per_deg * fabs(cos(yMin())) * width();
        double d1 = m_per_deg * fabs(cos(yMax())) * height();
        return Distance(std::max(d0, d1), Units::METERS).as(units);
    }
}

double
GeoExtent::height(const Units& units) const
{
    if (!valid())
        return 0.0;
    else if (getSRS()->isProjected()) {
        return Units::convert(getSRS()->getUnits(), units, width());
    }
    else {
        return Distance(
            getSRS()->getEllipsoid().longitudinalDegreesToMeters(height(), 0.0),
            Units::METERS).as(units);
    }
}

bool
GeoExtent::operator == ( const GeoExtent& rhs ) const
{
    if ( !valid() && !rhs.valid() )
        return true;

    if ( !valid() || !rhs.valid() )
        return false;

    return
        west()  == rhs.west()  &&
        east()  == rhs.east()  &&
        south() == rhs.south() &&
        north() == rhs.north() &&
        _srs->isEquivalentTo( rhs._srs.get() );
}

bool
GeoExtent::operator != ( const GeoExtent& rhs ) const
{
    return !( *this == rhs );
}

bool
GeoExtent::crossesAntimeridian() const
{
    return _srs && _srs->isGeographic() && east() < west();
}

bool
GeoExtent::splitAcrossAntimeridian(GeoExtent& out_west, GeoExtent& out_east) const
{
    if ( crossesAntimeridian() )
    {
        double width_new;

        out_west = *this;
        width_new = 180.0 - west();
        out_west.setOriginAndSize(180.0 - width_new, south(), width_new, height());

        out_east = *this;
        width_new = east() - (-180.0);
        out_east.setOriginAndSize(-180.0, south(), width_new, height());
    }
    else if ( !_srs->isGeographic() )
    {
        //note: may not actually work.
        GeoExtent latlong_extent = transform(_srs->getGeographicSRS());
        GeoExtent w, e;
        if (latlong_extent.splitAcrossAntimeridian(w, e))
        {
            out_west = w.transform(_srs);
            out_east = e.transform(_srs);
        }
    }

    return out_west.valid() && out_east.valid();
}

GeoExtent
GeoExtent::transform(shared_ptr<SRS> to_srs) const 
{
    // check for NULL
    if (!valid() || !to_srs)
        return GeoExtent::INVALID;

    // check for equivalence
    if (getSRS()->isHorizEquivalentTo(to_srs))
        return *this;

    //TODO: this may not work across the antimeridian - unit test required
    if (valid() && to_srs)
    {
        // do not normalize the X values here.
        double xmin = west(), ymin = south();
        double xmax = west() + width(), ymax = south() + height();

        if (_srs->transformExtentToMBR(to_srs, xmin, ymin, xmax, ymax))
        {
            return GeoExtent(to_srs, xmin, ymin, xmax, ymax);
        }
    }
    return GeoExtent::INVALID;
}


bool
GeoExtent::transform( shared_ptr<SRS> srs, GeoExtent& output ) const
{
    output = transform(srs);
    return output.valid();
}

void
GeoExtent::getBounds(double &xmin, double &ymin, double &xmax, double &ymax) const
{
    xmin = west();
    ymin = south();
    xmax = west() + width();
    ymax = south() + height();
}

Box
GeoExtent::bounds() const
{
    double west, east, south, north;
    getBounds(west, south, east, north);
    return Box( west, south, 0, east, north, 0 );
}

bool
GeoExtent::contains(double x, double y, shared_ptr<SRS> srs) const
{
    if (!valid() || !is_valid(x) || !is_valid(y))
        return false;

    dvec3 xy( x, y, 0 );
    dvec3 local(x, y, 0);
    shared_ptr<SRS> pSrs = _srs;

    // See if we need to xform the input:
    if (srs && srs->isHorizEquivalentTo(pSrs) == false)
    {
        // If the transform fails, bail out with error
       if (srs->transform(xy, pSrs, local) == false)
        {
            return false;
        }
    }

    const double epsilon = 1e-6;
    const double lsouth = south();
    const double lnorth = north();
    const double least = east();
    const double lwest = west();
    const double lwidth = width();
    double& localx = local.x;
    double& localy = local.y;

    // Quantize the Y coordinate to account for tiny rounding errors:
    if (fabs(lsouth - localy) < epsilon)
       localy = lsouth;
    if (fabs(lnorth - localy) < epsilon)
       localy = lnorth;

    // Test the Y coordinate:
    if (localy < lsouth || localy > lnorth)
        return false;

    // Bring the X coordinate into normal range:
    localx = normalizeX(localx);
    
    // Quantize the X coordinate to account for tiny rounding errors:
    if (fabs(lwest - localx) < epsilon)
       localx = lwest;
    if (fabs(least - localx) < epsilon)
       localx = least;

    // account for the antimeridian wrap-around:
    const double a0 = lwest, a1 = lwest + lwidth;
    const double b0 = least - lwidth, b1 = least;
    return (a0 <= localx && localx <= a1) || (b0 <= localx && localx <= b1);
}

bool
GeoExtent::contains(const GeoPoint& rhs) const
{
    return contains( rhs.x(), rhs.y(), rhs.getSRS() );
}

bool
GeoExtent::contains(const Box& rhs) const
{
    return
        valid() &&
        rhs.valid() &&
        contains( rhs.xmin, rhs.ymin ) &&
        contains( rhs.xmax, rhs.ymax ) &&
        contains( rhs.center() );
}

bool
GeoExtent::contains(const GeoExtent& rhs) const
{
    return
        valid() &&
        rhs.valid() &&
        contains( rhs.west(), rhs.south(), rhs.getSRS() ) &&
        contains( rhs.east(), rhs.north(), rhs.getSRS() ) &&
        contains( rhs.getCentroid().to_dvec3(), rhs.getSRS() );   // this accounts for the antimeridian
}

#undef  OVERLAPS
#define OVERLAPS(A, B, C, D) (!(B <= C || A >= D))

bool
GeoExtent::intersects(const GeoExtent& rhs, bool checkSRS) const
{
    if ( !valid() || !rhs.valid() )
        return false;

    // Transform the incoming extent if necessary:
    if (checkSRS && !_srs->isHorizEquivalentTo(rhs.getSRS().get()))
    {
        // non-contiguous projection? convert to a contiguous one:
        GeoExtent thisGeo = transform(getSRS()->getGeographicSRS());
        GeoExtent rhsGeo = rhs.transform(getSRS()->getGeographicSRS());
        return thisGeo.intersects(rhsGeo, false);
    }

    // Trivial reject: y-dimension does not overlap:
    bool y_excl = south() >= rhs.north() || north() <= rhs.south();
    if (y_excl)
        return false;

    // Trivial reject: x-dimension does not overlap in projected SRS:
    if (!_srs->isGeographic())
    {
        bool x_excl = west() >= rhs.east() || east() <= rhs.west();
        return x_excl == false;
    }

    // By now we know that Y overlaps and we are in a geographic SRS
    // and therefore must consider the antimeridian wrap-around in X.
    // a and b are "this"; c and d are "rhs":
    double a0 = east() - width(), a1 = east();
    double b0 = west(), b1 = west() + width();
    double c0 = rhs.east() - rhs.width(), c1 = rhs.east();
    double d0 = rhs.west(), d1 = rhs.west() + rhs.width();
    return
        OVERLAPS(a0, a1, c0, c1) ||
        OVERLAPS(a0, a1, d0, d1) ||
        OVERLAPS(b0, b1, c0, c1) ||
        OVERLAPS(b0, b1, d0, d1);
}

GeoCircle
GeoExtent::computeBoundingGeoCircle() const
{
    GeoCircle circle;

    if ( !valid() )
    {
        circle.setRadius( -1.0 );
    }
    else 
    {
        GeoPoint centroid = getCentroid();

        if ( getSRS()->isProjected() )
        {
            double ext = std::max( width(), height() );
            circle.setRadius( 0.5*ext * 1.414121356237 ); /*sqrt(2)*/
        }
        else // isGeographic
        {
            dvec3 center, sw, se, ne, nw;

            GeoPoint(getSRS(), centroid.x(), centroid.y(), 0, ALTMODE_ABSOLUTE).toWorld(center);
            GeoPoint(getSRS(), west(), south(), 0, ALTMODE_ABSOLUTE).toWorld(sw);
            GeoPoint(getSRS(), east(), south(), 0, ALTMODE_ABSOLUTE).toWorld(se);
            GeoPoint(getSRS(), east(), north(), 0, ALTMODE_ABSOLUTE).toWorld(ne);
            GeoPoint(getSRS(), west(), north(), 0, ALTMODE_ABSOLUTE).toWorld(nw);
            
            double radius2 = length_squared(center - sw);
            radius2 = std::max(radius2, length_squared(center - se));
            radius2 = std::max(radius2, length_squared(center - ne));
            radius2 = std::max(radius2, length_squared(center - sw));

            circle.setRadius(sqrt(radius2));
        }

        circle.setCenter(centroid);
    }

    return circle;
}

void
GeoExtent::expandToInclude(double x, double y)
{
    if (!is_valid(x) || !is_valid(y))
        return;

    // First, bring the X coordinate into the local frame.
    x = normalizeX(x);

    // Invalid? Set to a point.
    if (!valid())
    {
        set(x, y, x, y);
        return;
    }

    // Check each coordinate separately:
    GeoPoint centroid = getCentroid();
    bool containsX = contains(x, centroid.y());
    bool containsY = contains(centroid.x(), y);

    // Expand along the Y axis:
    if (!containsY)
    {
        if (y < south())
        {
            _height += (_south-y);
            _south = y;
        }
        else if (y > north())
        {
            _height = y - south();
        }
    }

    if (!containsX)
    {
        if (isGeographic())
        {
            if (x > west())
            {
                double w0 = x - west(); // non-wrap-around width
                double w1 = (180.0 - x) + (west() - (-180.0) + _width); // wrap-around width
                if (w0 <= w1)
                {
                    _width = w0;
                }
                else
                {
                    _west = x;
                    _width = w1;
                }
            }
            else // (x < west())
            {
                double w0 = _width + (west() - x); // non-wrap-around
                double w1 = (x - (-180.0)) + (180.0 - west()); // wrap-around
                if (w0 < w1)
                {
                    _west = x;
                    _width = w0;
                }
                else
                {
                    _width = w1;
                }
            }
        }
        else
        {
            // projected mode is the same approach as Y
            if (x < west())
            {
                _width += _west - x;
                _west = x;
            }
            else if (x > east())
            {
                _width = x - west();
            }
        }
    }

    if (!containsX || !containsY)
    {
        clamp();
    }
}

bool
GeoExtent::expandToInclude(const GeoExtent& rhs)
{
    if (!rhs.valid())
        return false;

    // no SRS? Just assign.
    if (!_srs)
    {
        *this = rhs;
        return true;
    }

    if (!rhs.getSRS()->isHorizEquivalentTo(_srs))
    {
        return expandToInclude(rhs.transform(_srs));
    }

    else
    {        
        if (area() <= 0.0)
        {
            *this = rhs;
            return true;
        }

        double h = std::max(north(), rhs.north()) - std::min(south(), rhs.south());
        if (rhs.south() < south())
        {
            _south = rhs.south();
        }
        _height = h;
        
        // non-wrap-around new width:
        double w0 = std::max(xMax(), rhs.xMax()) - std::min(xMin(), rhs.xMin());

        if (isGeographic())
        {
            // wrap-around width:
            double w1 = west() > rhs.east()? (180-west())+(rhs.east()-(-180)) : (180-rhs.west()) + (east()-(-180));

            // pick the smaller one:
            if (w0 <= w1)
            {
                if (w0 > _width)
                {
                    _width = w0;
                    _west = std::min(west(), rhs.west());
                }
            }
            else
            {
                if (w1 > _width)
                {
                    _width = w1;
                    if (west() <= rhs.east())
                        _west = rhs.west();
                }
            }
        }
        else
        {
            // projected mode is the same approach as Y
            _west = std::min(west(), rhs.west());
            _width = w0;
        }

    }

    return true;
}

namespace
{
    void sort4(double* n, bool* b)
    {
        if (n[0] > n[1]) std::swap(n[0], n[1]), std::swap(b[0], b[1]);
        if (n[2] > n[3]) std::swap(n[2], n[3]), std::swap(b[2], b[3]);
        if (n[0] > n[2]) std::swap(n[0], n[2]), std::swap(b[0], b[2]);
        if (n[1] > n[3]) std::swap(n[1], n[3]), std::swap(b[1], b[3]);
        if (n[1] > n[2]) std::swap(n[1], n[2]), std::swap(b[1], b[2]);
    }
}

GeoExtent
GeoExtent::intersectionSameSRS(const GeoExtent& rhs) const
{
    if ( !valid() || !rhs.valid() || !_srs->isHorizEquivalentTo( rhs.getSRS() ) )
        return GeoExtent::INVALID;

    if ( !intersects(rhs) )
    {
        return GeoExtent::INVALID;
    }

    GeoExtent result( *this );

    if (isGeographic())
    {
        if (width() == 360.0)
        {
            result._west = rhs._west;
            result._width = rhs._width;
        }
        else if (rhs.width() == 360.0)
        {
            result._west = _west;
            result._width = _width;
        }
        else
        {
            // Sort the four X coordinates, remembering whether each one is west or east edge:
            double x[4];
            bool iswest[4];
            x[0] = west(), x[1] = east(), x[2] = rhs.west(), x[3] = rhs.east();
            iswest[0] = true, iswest[1] = false, iswest[2] = true, iswest[3] = false;
            sort4(x, iswest);

            // find the western-most west coord:
            int iw = -1;
            for (int i=0; i<4 && iw<0; ++i)
            {
                if (iswest[i])
                    iw = i;
            }

            // iterate from there, finding the LAST west coord and stopping on the 
            // FIRST east coord found.
            int q = iw+4;
            int ie = -1;
            for (int i = iw; i < q && ie < 0; ++i)
            {
                int j = i;
                if (j >= 4) j-=4;
                if (iswest[j])
                    iw = j; // found a better west coord; remember it.
                else
                    ie = j; // found the western-most east coord; done.
            }

            result._west = x[iw];
            if (ie >= iw)
                result._width = x[ie] - x[iw];
            else
                result._width = (180.0 - x[iw]) + (x[ie] - (-180.0)); // crosses the antimeridian
        }
    }
    else
    {
        // projected mode is simple
        result._west = std::max(west(), rhs.west());
        double eastTemp = std::min(east(), rhs.east());
        result._width = eastTemp - result._west;
    }

    result._south = std::max(south(), rhs.south());
    double northTemp = std::min(north(), rhs.north());
    result._height = northTemp - result._south;

    result.clamp();

    return result;
}

void
GeoExtent::scale(double x_scale, double y_scale)
{
    if (!valid() || !is_valid(x_scale) || !is_valid(y_scale))
        return;

    double cx = _west + 0.5*_width;

    double cy = _south + 0.5*_height;
     
    setOriginAndSize(
        normalizeX(cx - 0.5*_width*x_scale),
        cy - 0.5*_height*y_scale,
        _width * x_scale,
        _height * y_scale);
}

void
GeoExtent::expand(double x, double y)
{
    if (!_srs || !is_valid(x) || !is_valid(y))
        return;

    setOriginAndSize(
        normalizeX(_west - 0.5*x),
        _south - 0.5*y,
        _width + x,
        _height + y);
}

void
GeoExtent::expand(const Distance& x, const Distance& y)
{
    if (!_srs)
        return;

    double latitude = valid() ? (yMin() >= 0.0 ? yMin() : yMax()) : 0.0;

    double xp = SRS::transformUnits(x, _srs, Angle(latitude, Units::DEGREES));
    double yp = SRS::transformUnits(y, _srs, Angle());

    expand(xp, yp);
}

void
GeoExtent::clamp()
{
    if (equivalent(_west, floor(_west)))
        _west = floor(_west);
    else if (equivalent(_west, ceil(_west)))
        _west = ceil(_west);

    if (equivalent(_south, floor(_south)))
        _south = floor(_south);
    else if (equivalent(_south, ceil(_south)))
        _south = ceil(_south);

    if (equivalent(_width, floor(_width)))
        _width = floor(_width);
    else if (equivalent(_width, ceil(_width)))
        _width = ceil(_width);

    if (equivalent(_height, floor(_height)))
        _height = floor(_height);
    else if (equivalent(_height, ceil(_height)))
        _height = ceil(_height);

    if (isGeographic())
    {
        _width = rocky::clamp(_width, 0.0, 360.0);
        //_height = osg::clampBetween(_height, 0.0, 180.0);

        if (south() < -90.0)
        {
            _height -= (-90.0)-_south;
            _south = -90.0;
        }
        else if (north() > 90.0)
        {
            _height -= (north()-90.0);
        }

        _height = rocky::clamp(_height, 0.0, 180.0);
    }
}

double
GeoExtent::area() const
{
    return valid() ? width() * height() : 0.0;
}

double
GeoExtent::normalizeX(double x) const
{
    if (is_valid(x) && _srs && _srs->isGeographic())
    {
        if (fabs(x) <= 180.0)
        {
            return x;
        }

        if (x < 0.0 || x >= 360.0)
        {
            x = fmod(x, 360.0);
            if (x < 0.0)
                x += 360.0;
        }
        
        if (x > 180.0)
        {
            x -= 360.0;
        }
    }
    return x;
}

std::string
GeoExtent::toString() const
{
    std::stringstream buf;
    if ( !valid() )
        buf << "INVALID";
    else
        buf << std::setprecision(12) << "SW=" << west() << "," << south() << " NE=" << east() << "," << north();

    if (_srs)
    {
        buf << ", SRS=" << _srs->getName();
    }
    else
    {
        buf << ", SRS=NULL";
    }

    std::string bufStr;
    bufStr = buf.str();
    return bufStr;
}

#if 0
bool
GeoExtent::createPolytope(osg::Polytope& tope) const
{
    if ( ! this->valid() )
        return false;

    if ( getSRS()->isProjected() )
    {
        // add planes for the four sides of the extent, Normals point inwards.
        double halfWidth  = 0.5*width();
        double halfHeight = 0.5*height();
        tope.add( osg::Plane(dvec3( 1, 0,0), dvec3(-halfWidth,0,0)) );
        tope.add( osg::Plane(dvec3(-1, 0,0), dvec3( halfWidth,0,0)) );
        tope.add( osg::Plane(dvec3( 0, 1,0), dvec3(0, -halfHeight,0)) );
        tope.add( osg::Plane(dvec3( 0,-1,0), dvec3(0,  halfHeight,0)) );
    }

    else
    {
        // for each extent, create a plane passing through the planet's centroid.

        // convert 4 corners to world space (ECEF)
        dvec3 center(0.0, 0.0, 0.0);
        dvec3 sw, se, nw, ne;
        GeoPoint(getSRS(), west(), south(), 0.0, ALTMODE_ABSOLUTE).toWorld( sw );
        GeoPoint(getSRS(), east(), south(), 0.0, ALTMODE_ABSOLUTE).toWorld( se );
        GeoPoint(getSRS(), east(), north(), 0.0, ALTMODE_ABSOLUTE).toWorld( ne );
        GeoPoint(getSRS(), west(), north(), 0.0, ALTMODE_ABSOLUTE).toWorld( nw );

        // bounding planes in ECEF space:
        tope.add( osg::Plane(center, nw, sw) ); // west
        tope.add( osg::Plane(center, sw, se) ); // south
        tope.add( osg::Plane(center, se, ne) ); // east
        tope.add( osg::Plane(center, ne, nw) ); // north
    }

    return true;
}
#endif

Sphere
GeoExtent::createWorldBoundingSphere(double minElev, double maxElev) const
{
    Sphere bs;

    if (getSRS()->isProjected())
    {
        dvec3 w;
        GeoPoint(getSRS(), xMin(), yMin(), minElev, ALTMODE_ABSOLUTE).toWorld(w); bs.expandBy(w);
        GeoPoint(getSRS(), xMax(), yMax(), maxElev, ALTMODE_ABSOLUTE).toWorld(w); bs.expandBy(w);
    }
    else // geocentric
    {
        // Sample points along the extent
        std::vector< dvec3 > samplePoints;

        int samples = 7;

        double xSample = width() / (double)(samples - 1);
        double ySample = height() / (double)(samples - 1);

        for (int c = 0; c < samples; c++)
        {
            double x = xMin() + (double)c * xSample;
            for (int r = 0; r < samples; r++)
            {
                double y = yMin() + (double)r * ySample;
                
                dvec3 world;
                GeoPoint(getSRS(), x, y, minElev, ALTMODE_ABSOLUTE).toWorld(world);
                samplePoints.push_back(world);               
                GeoPoint(getSRS(), x, y, maxElev, ALTMODE_ABSOLUTE).toWorld(world);
                samplePoints.push_back(world);
            }
        }

        // Compute the bounding box of the sample points
        Box bb;
        for (auto& p : samplePoints)
        {
            bb.expandBy(p);
        }

        // The center of the bounding sphere is the center of the bounding box
        bs.center = bb.center();

        // Compute the max radius based on the distance from the bounding boxes center.
        double maxRadius = -DBL_MAX;

        for (auto& p : samplePoints)
        {
            double r = (p - bs.center).length();
            if (r > maxRadius) maxRadius = r;
        }

        bs.radius = maxRadius;
    }

    return bs;
}

bool
GeoExtent::createScaleBias(const GeoExtent& rhs, dmat4& output) const
{
    double scalex = width() / rhs.width();
    double scaley = height() / rhs.height();
    double biasx = (west() - rhs.west()) / rhs.width();
    double biasy = (south() - rhs.south()) / rhs.height();

    // TODO: is the row/column right for GLM?
    ROCKY_WARN << "Check the dmat4" << std::endl;
    //output = dmat4(
    //    scalex, 0, 0, 0,
    //    0, scaley, 0, 0,
    //    0, 0, 1, 0,
    //    biasx, biasy, 0, 1);

    output = dmat4(1);
    output[0][0] = scalex;
    output[1][1] = scaley;
    output[3][0] = biasx;  // [0][3]?
    output[3][1] = biasy;  // [1][3]?

    return true;
}

/***************************************************************************/

DataExtent::DataExtent() :
    GeoExtent()
{
    //NOP
}

DataExtent::DataExtent(const GeoExtent& extent, unsigned minLevel, unsigned maxLevel, const std::string &description) :
GeoExtent(extent)
{
    _minLevel = minLevel;
    _maxLevel = maxLevel;
    _description = description;
}

DataExtent::DataExtent(const GeoExtent& extent, const std::string &description) :
    GeoExtent(extent),
    _minLevel(0),
    _maxLevel(19u)
{
    _description = description;
}

DataExtent::DataExtent(const GeoExtent& extent, unsigned minLevel, unsigned maxLevel) :
    GeoExtent(extent)
{
    _minLevel = minLevel;
    _maxLevel = maxLevel;
}

DataExtent::DataExtent(const GeoExtent& extent, unsigned minLevel) :
    GeoExtent(extent),
    _maxLevel(19u)
{
    _minLevel = minLevel;
}

DataExtent::DataExtent(const GeoExtent& extent, unsigned minLevel, const std::string &description) :
    GeoExtent(extent),
    _maxLevel(19u)
{
    _minLevel = minLevel;
    _description = description;
}

DataExtent::DataExtent(const GeoExtent& extent) :
    GeoExtent(extent),
    _minLevel(0),
    _maxLevel(19u)
{
    //nop
}
