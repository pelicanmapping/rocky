#include "EntityPosition.h"
#include "Math.h"
#include "Utils.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef  LC
#define LC "[EntityPosition] "

EntityPosition EntityPosition::INVALID;

EntityPosition::EntityPosition()
{
    //nop
}

EntityPosition&
EntityPosition::operator=(EntityPosition&& rhs)
{
    basePosition = rhs.basePosition;
    altitude = rhs.altitude;
    rhs.basePosition = { }; // invalidate rhs
    return *this;
}

EntityPosition::EntityPosition(const GeoPoint& in_basePosition, double in_altitude) :
    basePosition(in_basePosition),
    altitude(in_altitude)
{
    //nop
}

bool
EntityPosition::transform(const SRS& outSRS, EntityPosition& output) const
{
    if (valid() && outSRS.valid())
    {
        GeoPoint outBasePosition;
        if (basePosition.transform(outSRS, outBasePosition))
        {
            output = EntityPosition(outBasePosition, altitude);
            return true;
        }
    }
    return false;
}

bool
EntityPosition::transformInPlace(const SRS& to_srs)
{
    if (valid() && to_srs.valid())
    {
        if (basePosition.transformInPlace(to_srs))
        {
            return true;
        }
    }
    return false;
}

#include "json.h"

namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const EntityPosition& obj) {
        if (obj.valid()) {
            j = json::object();
            set(j, "basePosition", obj.basePosition);
            set(j, "altitude", obj.altitude);
        }
    }

    void from_json(const json& j, EntityPosition& obj) {
        GeoPoint basePosition;
        double altitude = 0;

        get_to(j, "basePosition", basePosition);
        get_to(j, "altitude", altitude);

        obj = EntityPosition(basePosition, altitude);
    }
}
