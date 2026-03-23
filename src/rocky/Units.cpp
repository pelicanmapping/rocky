/* rocky
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "Units.h"
#include "Utils.h"
#include "IOTypes.h"
#include <string>

using namespace ROCKY_NAMESPACE;

UnitsParser::UnitsParser()
{
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
        _table[ptr->name()] = *ptr;
    }
}

std::optional<UnitsType>
UnitsParser::parseUnits(std::string_view name) const
{
    std::unique_lock lock(_mutex);
    auto i = _table.find(name.data());
    if (i != _table.end())
        return i->second;
    else
        return std::nullopt;
}

std::optional<QualifiedValue>
UnitsParser::parse(std::string_view input, const UnitsType& defaultUnits) const
{
    QualifiedValue out;
    char* endptr = nullptr;

    // parse the numeric part into "value", and point "pos" at the first
    // non-numeric character in the string.

    out.value = std::strtod(input.data(), &endptr);

    if (std::isnan(out.value))
        return std::nullopt;

    std::string unitsStr = endptr ? detail::trim(endptr) : "";

    if (unitsStr.empty())
    {
        out.units = defaultUnits;
    }
    else
    {
        auto units = parseUnits(unitsStr);
        if (!units.has_value())
        {
            return std::nullopt;
        }

        out.units = units.value();
    }

    return out;
}

int
UnitsParser::unitTest() const
{
    // test parsing scientific notation
    {
        auto out = parse("123e-003m", Units::MILES);
        if (!out.has_value() || out->value != 123e-003 || out->units != Units::METERS)
            return 101;
    }
    {
        auto out = parse("123e+003m", Units::MILES);
        if (!out.has_value() || out->value != 123e+003 || out->units != Units::METERS)
            return 102;
    }
    {
        auto out = parse("123E-003m", Units::MILES);
        if (!out.has_value() || out->value != 123E-003 || out->units != Units::METERS)
            return 103;
    }
    {
        auto out = parse("123E+003m", Units::MILES);
        if (!out.has_value() || out->value != 123E+003 || out->units != Units::METERS)
            return 104;
    }

    // normal parsing
    {
        auto out = parse("123m", Units::MILES);
        if (!out.has_value() || out->value != 123 || out->units != Units::METERS)
            return 201;
    }
    {
        auto out = parse("123km", Units::MILES);
        if (!out.has_value() || out->value != 123 || out->units != Units::KILOMETERS)
            return 202;
    }
    {
        auto out = parse("1.2rad", Units::DEGREES);
        if (!out.has_value() || out->value != 1.2 || out->units != Units::RADIANS)
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
        auto v = UnitsParser().parse(get_string(j), Units::METERS);
        if (v.has_value()) obj = Distance(v->value, v->units);
    }

    void to_json(json& j, const Angle& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Angle& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::DEGREES);
        if (v.has_value()) obj = Angle(v->value, v->units);
    }

    void to_json(json& j, const Duration& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Duration& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::SECONDS);
        if (v.has_value()) obj = Duration(v->value, v->units);
    }

    void to_json(json& j, const Speed& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, Speed& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::METERS_PER_SECOND);
        if (v.has_value()) obj = Speed(v->value, v->units);
    }

    void to_json(json& j, const ScreenSize& obj) {
        j = obj.to_parseable_string();
    }
    void from_json(const json& j, ScreenSize& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::PIXELS);
        if (v.has_value()) obj = ScreenSize(v->value, v->units);
    }
}
