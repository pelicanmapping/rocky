/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Math.h"
#include <algorithm>
#include <float.h>

using namespace ROCKY_NAMESPACE;

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
