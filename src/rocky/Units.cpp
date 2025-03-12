/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Units.h"
#include "Utils.h"
#include <mutex>
#include <string>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

namespace
{
    static std::unordered_map<std::string, const Units*> s_unitsTable;
    static std::mutex s_unitsTable_mutex;

    template<typename T>
    bool parseValueAndUnits(const std::string& input, T& out_value, Units& out_units, const Units& defaultUnits)
    {
        // parse the numeric part into "value", and point "ptr" at the first
        // non-numeric character in the string.
        std::size_t pos;
        try {
            out_value = std::stod(input.c_str(), &pos);
            if (std::isnan(out_value))
                return false;
        }
        catch (...) {
            return false;
        }

        std::string unitsStr = trim(input.substr(pos));

        if (unitsStr.empty())
        {
            out_units = defaultUnits;
            //return false;
        }
        else
        {
            Units units;
            if (Units::parse(unitsStr, units))
                out_units = units;
            else if (unitsStr.back() != 's' && Units::parse(unitsStr + 's', units))
                out_units = units;
            else {
                //error!
                return false;
            }
        }

        return true;
    }
}

//------------------------------------------------------------------------



Units::Units( const std::string& name, const std::string& abbr, const Units::Type& type, double toBase ) :
_name  ( name ),
_abbr  ( abbr ),
_type  ( type ),
_toBase( toBase ),
_distance(0L),
_time(0L)
{
    //nop
}

Units::Units( const std::string& name, const std::string& abbr, const Units& distance, const Units& time ) :
_name    ( name ),
_abbr    ( abbr ),
_type    ( TYPE_SPEED ),
_toBase  ( 1.0 ),
_distance( &distance ),
_time    ( &time )
{
    //nop
}

bool
Units::parse(const std::string& name, Units& output)
{
    std::unique_lock lock(s_unitsTable_mutex);
    auto i = s_unitsTable.find(name);
    if (i != s_unitsTable.end())
    {
        output = *(i->second);
        return true;
    }
    return false;
}

bool
Units::parse( const std::string& input, float& out_value, Units& out_units, const Units& defaultUnits )
{
    return parseValueAndUnits(input, out_value, out_units, defaultUnits);
}

bool
Units::parse( const std::string& input, double& out_value, Units& out_units, const Units& defaultUnits )
{
    return parseValueAndUnits(input, out_value, out_units, defaultUnits);
}

// Factor converts unit into METERS:
const Units Units::CENTIMETERS       ( "centimeters",    "cm",  Units::TYPE_LINEAR, 0.01 ); 
const Units Units::FEET              ( "feet",           "ft",  Units::TYPE_LINEAR, 0.3048 );
const Units Units::FEET_US_SURVEY    ( "feet(us)",       "ft",  Units::TYPE_LINEAR, 12.0/39.37 );
const Units Units::KILOMETERS        ( "kilometers",     "km",  Units::TYPE_LINEAR, 1000.0 );
const Units Units::METERS            ( "meters",         "m",   Units::TYPE_LINEAR, 1.0 );
const Units Units::MILES             ( "miles",          "mi",  Units::TYPE_LINEAR, 1609.334 );
const Units Units::MILLIMETERS       ( "millimeters",    "mm",  Units::TYPE_LINEAR, 0.001 );
const Units Units::YARDS             ( "yards",          "yd",  Units::TYPE_LINEAR, 0.9144 );
const Units Units::NAUTICAL_MILES    ( "nautical miles", "nm",  Units::TYPE_LINEAR, 1852.0 );
const Units Units::DATA_MILES        ( "data miles",     "dm",  Units::TYPE_LINEAR, 1828.8 );
const Units Units::INCHES            ( "inches",         "in",  Units::TYPE_LINEAR, 0.0254 );
const Units Units::FATHOMS           ( "fathoms",        "fm",  Units::TYPE_LINEAR, 1.8288 );
const Units Units::KILOFEET          ( "kilofeet",       "kf",  Units::TYPE_LINEAR, 304.8 );
const Units Units::KILOYARDS         ( "kiloyards",      "kyd", Units::TYPE_LINEAR, 914.4 );

// Factor converts unit into RADIANS:
const Units Units::DEGREES           ( "degrees",        "\xb0",Units::TYPE_ANGULAR, 0.017453292519943295 );
const Units Units::RADIANS           ( "radians",        "rad", Units::TYPE_ANGULAR, 1.0 );
const Units Units::BAM               ( "BAM",            "bam", Units::TYPE_ANGULAR, 6.283185307179586476925286766559 );
const Units Units::NATO_MILS         ( "mils",           "mil", Units::TYPE_ANGULAR, 9.8174770424681038701957605727484e-4 );
const Units Units::DECIMAL_HOURS     ( "hours",          "h",   Units::TYPE_ANGULAR, 15.0*0.017453292519943295 );

// Factor convert unit into SECONDS:
const Units Units::DAYS              ( "days",           "d",   Units::TYPE_TEMPORAL, 86400.0 );
const Units Units::HOURS             ( "hours",          "hr",  Units::TYPE_TEMPORAL, 3600.0 );
const Units Units::MICROSECONDS      ( "microseconds",   "us",  Units::TYPE_TEMPORAL, 0.000001 );
const Units Units::MILLISECONDS      ( "milliseconds",   "ms",  Units::TYPE_TEMPORAL, 0.001 );
const Units Units::MINUTES           ( "minutes",        "min", Units::TYPE_TEMPORAL, 60.0 );
const Units Units::SECONDS           ( "seconds",        "s",   Units::TYPE_TEMPORAL, 1.0 );
const Units Units::WEEKS             ( "weeks",          "wk",  Units::TYPE_TEMPORAL, 604800.0 );

const Units Units::FEET_PER_SECOND      ( "feet per second",         "ft/s", Units::FEET,           Units::SECONDS );
const Units Units::YARDS_PER_SECOND     ( "yards per second",        "yd/s", Units::YARDS,          Units::SECONDS );
const Units Units::METERS_PER_SECOND    ( "meters per second",       "m/s",  Units::METERS,         Units::SECONDS );
const Units Units::KILOMETERS_PER_SECOND( "kilometers per second",   "km/s", Units::KILOMETERS,     Units::SECONDS );
const Units Units::KILOMETERS_PER_HOUR  ( "kilometers per hour",     "kmh",  Units::KILOMETERS,     Units::HOURS );
const Units Units::MILES_PER_HOUR       ( "miles per hour",          "mph",  Units::MILES,          Units::HOURS );
const Units Units::DATA_MILES_PER_HOUR  ( "data miles per hour",     "dm/h", Units::DATA_MILES,     Units::HOURS );
const Units Units::KNOTS                ( "nautical miles per hour", "kts",  Units::NAUTICAL_MILES, Units::HOURS );

const Units Units::PIXELS               ( "pixels", "px", Units::TYPE_SCREEN_SIZE, 1.0 );


void
Units::registerAll()
{
    std::unique_lock lock(s_unitsTable_mutex);

    auto units = {
        &Units::CENTIMETERS,
        &Units::FEET,
        &Units::FEET_US_SURVEY,
        &Units::KILOMETERS,
        &Units::METERS,
        &Units::MILES,
        &Units::MILLIMETERS,
        &Units::YARDS,
        &Units::NAUTICAL_MILES,
        &Units::DATA_MILES,
        &Units::INCHES,
        &Units::FATHOMS,
        &Units::KILOFEET,
        &Units::KILOYARDS,

        &Units::DEGREES,
        &Units::RADIANS,
        &Units::BAM,
        &Units::NATO_MILS,
        &Units::DECIMAL_HOURS,

        &Units::DAYS,
        &Units::HOURS,
        &Units::MICROSECONDS,
        &Units::MILLISECONDS,
        &Units::MINUTES,
        &Units::SECONDS,
        &Units::WEEKS,

        &Units::FEET_PER_SECOND,
        &Units::YARDS_PER_SECOND,
        &Units::METERS_PER_SECOND,
        &Units::KILOMETERS_PER_SECOND,
        &Units::KILOMETERS_PER_HOUR,
        &Units::MILES_PER_HOUR,
        &Units::DATA_MILES_PER_HOUR,
        &Units::KNOTS,

        &Units::PIXELS
    };

    for (auto& ptr : units)
    {
        s_unitsTable[util::toLower(ptr->getName())] = ptr;
    }
}


int
Units::unitTest()
{
    double value;
    Units  units;

    // test parsing scientific notation
    {
        Units::parse( "123e-003m", value, units, Units::MILES);
        if ( value != 123e-003 || units != Units::METERS )
            return 101;

        Units::parse( "123e+003m", value, units, Units::MILES );
        if ( value != 123e+003 || units != Units::METERS )
            return 102;

        Units::parse( "123E-003m", value, units, Units::MILES );
        if ( value != 123E-003 || units != Units::METERS )
            return 103;

        Units::parse( "123E+003m", value, units, Units::MILES );
        if ( value != 123E+003 || units != Units::METERS )
            return 104;
    }

    // normal parsing
    {
        Units::parse( "123m", value, units, Units::MILES );
        if ( value != 123 || units != Units::METERS )
            return 201;
        
        Units::parse( "123km", value, units, Units::MILES );
        if ( value != 123 || units != Units::KILOMETERS )
            return 202;
        
        Units::parse( "1.2rad", value, units, Units::DEGREES );
        if ( value != 1.2 || units != Units::RADIANS )
            return 203;
    }

    // add tests as needed

    return 0;
}


#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const Distance& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Distance& obj) {
        obj = Distance(get_string(j), Units::METERS);
    }

    void to_json(json& j, const Angle& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Angle& obj) {
        obj = Angle(get_string(j), Units::DEGREES);
    }

    void to_json(json& j, const Duration& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Duration& obj) {
        obj = Duration(get_string(j), Units::SECONDS);
    }

    void to_json(json& j, const Speed& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Speed& obj) {
        obj = Speed(get_string(j), Units::METERS_PER_SECOND);
    }

    void to_json(json& j, const ScreenSize& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, ScreenSize& obj) {
        obj = ScreenSize(get_string(j), Units::PIXELS);
    }
}
