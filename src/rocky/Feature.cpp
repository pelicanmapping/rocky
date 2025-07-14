/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Feature.h"
#include <stack>

#ifdef ROCKY_HAS_GDAL
#include <gdal.h> // OGR API
#include <ogr_spatialref.h>
#endif

using namespace ROCKY_NAMESPACE;


void
Geometry::convertToType(Type in_type)
{
    if (in_type != type)
    {
        Type multi_variation =
            in_type == Type::Points ? Type::MultiPoints :
            in_type == Type::LineString ? Type::MultiLineString :
            Type::MultiPolygon;

        std::stack<Geometry*> stack;
        stack.push(this);
        while(!stack.empty())
        {
            auto& geom = *stack.top();
            stack.pop();

            if (geom.type >= Type::MultiPoints)
            {
                geom.type = multi_variation;
            }
            else
            {
                geom.type = in_type;
            }

            if (geom.parts.size() > 0)
            {
                for (auto& part : geom.parts)
                {
                    stack.push(&part);
                }
            }
        }
    }
}

std::string
Geometry::typeToString(Geometry::Type type)
{
    return
        type == Geometry::Type::Points ? "Points" :
        type == Geometry::Type::LineString ? "LineString" :
        type == Geometry::Type::Polygon ? "Polygon" :
        type == Geometry::Type::MultiPoints ? "MultiPoints" :
        type == Geometry::Type::MultiLineString ? "MultiLineString" :
        type == Geometry::Type::MultiPolygon ? "MultiPolygon" :
        "Unknown";
}

namespace
{
    template<class T>
    bool ring_contains(const T& points, double x, double y)
    {
        bool result = false;
        bool is_open = (points.size() > 1 && points.front() != points.back());
        unsigned i = is_open ? 0 : 1;
        unsigned j = is_open ? (unsigned)points.size() - 1 : 0;
        for (; i < (unsigned)points.size(); j = i++)
        {
            if ((((points[i].y <= y) && (y < points[j].y)) ||
                ((points[j].y <= y) && (y < points[i].y))) &&
                (x < (points[j].x - points[i].x) * (y - points[i].y) / (points[j].y - points[i].y) + points[i].x))
            {
                result = !result;
            }
        }
        return result;
    }
}

bool
Geometry::contains(double x, double y) const
{
    if (type == Type::Polygon)
    {
        if (!ring_contains(points, x, y))
            return false;

        for (auto& hole : parts)
            if (ring_contains(hole.points, x, y))
                return false;

        return true;
    }
    else if (type == Type::MultiPolygon)
    {
        Geometry::const_iterator iter(*this, false);
        while (iter.hasMore())
        {
            if (iter.next().contains(x, y))
                return true;
        }
        return false;
    }
    else
    {
        return false;
    }
}

void
Feature::dirtyExtent()
{
    Box box;

    geometry.eachPart([&](const Geometry& part)
        {
            for (const auto& point : part.points)
            {
                box.expandBy(point);
            }
        });

    extent = GeoExtent(srs, box);
}

bool
Feature::transformInPlace(const SRS& to_srs)
{
    if (srs == to_srs)
        return true;
    if (!srs.valid() || !to_srs.valid())
        return false;

    // Transform the geometry points:
    geometry.eachPart([&](Geometry& part)
        {
            srs.to(to_srs).transformRange(part.points.begin(), part.points.end());
        });

    // Update the SRS:
    srs = to_srs;
    // Recompute the extent:
    dirtyExtent();
    return true;
}
