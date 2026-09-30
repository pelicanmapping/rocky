/* rocky
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "Units.h"
#include "json.h"
#include <charconv>
#include <cmath>
#include <limits>

using namespace ROCKY_NAMESPACE;

namespace
{
    using Domain = Units::Domain;

    struct UnitInfo
    {
        Domain domain;
        double toBase;
        std::string_view name;
        std::string_view abbr;
    };

    // Indexed by Units::Type. Preserve the existing scales; speed scales use meters/second.
    constexpr UnitInfo unitInfo[] = {
        { Domain::INVALID, 0.0, {}, {} },
        { Domain::DISTANCE, 0.01, "centimeters", "cm" },
        { Domain::DISTANCE, 0.3048, "feet", "ft" },
        { Domain::DISTANCE, 12.0 / 39.37, "feet(us)", "ft(us)" },
        { Domain::DISTANCE, 1000.0, "kilometers", "km" },
        { Domain::DISTANCE, 1.0, "meters", "m" },
        { Domain::DISTANCE, 1609.334, "miles", "mi" },
        { Domain::DISTANCE, 0.001, "millimeters", "mm" },
        { Domain::DISTANCE, 0.9144, "yards", "yd" },
        { Domain::DISTANCE, 1852.0, "nautical miles", "nm" },
        { Domain::DISTANCE, 1828.8, "data miles", "dm" },
        { Domain::DISTANCE, 0.0254, "inches", "in" },
        { Domain::DISTANCE, 1.8288, "fathoms", "fm" },
        { Domain::DISTANCE, 304.8, "kilofeet", "kf" },
        { Domain::DISTANCE, 914.4, "kiloyards", "kyd" },
        { Domain::ANGLE, 0.017453292519943295, "degrees", "deg" },
        { Domain::ANGLE, 1.0, "radians", "rad" },
        { Domain::ANGLE, 6.283185307179586476925286766559, "BAM", "bam" },
        { Domain::ANGLE, 9.8174770424681038701957605727484e-4, "mils", "mil" },
        { Domain::ANGLE, 15.0 * 0.017453292519943295, "decimal hours", "h" },
        { Domain::TIME, 86400.0, "days", "d" },
        { Domain::TIME, 3600.0, "hours", "hr" },
        { Domain::TIME, 0.000001, "microseconds", "us" },
        { Domain::TIME, 0.001, "milliseconds", "ms" },
        { Domain::TIME, 60.0, "minutes", "min" },
        { Domain::TIME, 1.0, "seconds", "s" },
        { Domain::TIME, 604800.0, "weeks", "wk" },
        { Domain::SPEED, 0.3048, "feet per second", "ft/s" },
        { Domain::SPEED, 0.9144, "yards per second", "yd/s" },
        { Domain::SPEED, 1.0, "meters per second", "m/s" },
        { Domain::SPEED, 1000.0, "kilometers per second", "km/s" },
        { Domain::SPEED, 1000.0 / 3600.0, "kilometers per hour", "kmh" },
        { Domain::SPEED, 1609.334 / 3600.0, "miles per hour", "mph" },
        { Domain::SPEED, 1828.8 / 3600.0, "data miles per hour", "dm/h" },
        { Domain::SPEED, 1852.0 / 3600.0, "nautical miles per hour", "kts" },
        { Domain::SCREEN, 1.0, "pixels", "px" }
    };
    static_assert(sizeof(unitInfo) / sizeof(unitInfo[0]) == Units::NUM_TYPES, "Missing unit metadata");

    struct UnitAlias
    {
        std::string_view text;
        Units::Type type;
    };

    // Additional accepted spellings, including both legacy and UTF-8 degree symbols.
    constexpr UnitAlias unitAliases[] = {
        { "\xb0", Units::DEGREES },
        { "\xc2\xb0", Units::DEGREES },
        { "km/h", Units::KILOMETERS_PER_HOUR },
        { "knots", Units::KNOTS },
        { "ftUS", Units::FEET_US_SURVEY }
    };

    //! Returns shared metadata, mapping all unsupported identifiers to the invalid entry.
    constexpr const UnitInfo& info(Units units) noexcept
    {
        return unitInfo[units.valid() ? units.type : Units::INVALID];
    }

    //! Removes ASCII whitespace without allocating or reading beyond a string view.
    std::string_view trimUnitsInput(std::string_view input) noexcept
    {
        constexpr std::string_view whitespace = " \t\r\n\f\v";
        auto first = input.find_first_not_of(whitespace);
        if (first == input.npos)
            return {};
        return input.substr(first, input.find_last_not_of(whitespace) - first + 1);
    }
}

std::string_view
Units::name() const noexcept
{
    return info(*this).name;
}

std::string_view
Units::abbr() const noexcept
{
    return info(*this).abbr;
}

Units::Domain
Units::domain() const noexcept
{
    return info(*this).domain;
}

bool
Units::canConvert(Units from, Units to) noexcept
{
    return from.valid() && to.valid() && info(from).domain == info(to).domain;
}

std::optional<double>
Units::conversionFactor(Units from, Units to) noexcept
{
    if (!canConvert(from, to))
        return std::nullopt;
    return info(from).toBase / info(to).toBase;
}

bool
Units::convert(Units from, Units to, double input, double& output) noexcept
{
    auto factor = conversionFactor(from, to);
    if (!factor)
        return false;
    output = input * *factor;
    return true;
}

double
Units::convert(Units from, Units to, double input) noexcept
{
    double output = std::numeric_limits<double>::quiet_NaN();
    convert(from, to, input, output);
    return output;
}

std::optional<Units>
UnitsParser::parseUnits(std::string_view input) const
{
    input = trimUnitsInput(input);
    if (input.empty())
        return std::nullopt;

    // This small, fixed catalog needs neither heap allocations nor mutable lookup state.
    for (unsigned i = 1; i < Units::NUM_TYPES; ++i)
    {
        const auto& entry = unitInfo[i];
        if (input == entry.name || input == entry.abbr)
            return Units(static_cast<Units::Type>(i));
    }
    for (const auto& alias : unitAliases)
    {
        if (input == alias.text)
            return Units(alias.type);
    }
    return std::nullopt;
}

std::optional<QualifiedValue>
UnitsParser::parse(std::string_view input, Units defaultUnits) const
{
    input = trimUnitsInput(input);
    if (input.empty())
        return std::nullopt;

    // Unlike strtod, from_chars respects the view's end and is independent of the process locale.
    // It does not accept a leading plus sign, so handle that without accepting two signs.
    if (input.front() == '+')
    {
        input.remove_prefix(1);
        if (input.empty() || input.front() == '+' || input.front() == '-')
            return std::nullopt;
    }

    QualifiedValue out;
    auto number = std::from_chars(input.data(), input.data() + input.size(), out.value);
    if (number.ec != std::errc{} || !std::isfinite(out.value))
        return std::nullopt;

    auto suffix = trimUnitsInput(input.substr(static_cast<std::size_t>(number.ptr - input.data())));
    if (suffix.empty())
        out.units = defaultUnits;
    else
    {
        auto parsed = parseUnits(suffix);
        if (!parsed)
            return std::nullopt;
        out.units = *parsed;
    }
    return out.units.valid() ? std::optional<QualifiedValue>(out) : std::nullopt;
}

std::string
ROCKY_NAMESPACE::detail::formatQualifiedValue(double value, Units units)
{
    if (!units.valid() || !std::isfinite(value))
        return {};

    char buffer[128];
    auto number = std::to_chars(buffer, buffer + sizeof(buffer), value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (number.ec != std::errc{})
        return {};
    std::string result(buffer, number.ptr);
    result.append(units.abbr());
    return result;
}

namespace ROCKY_NAMESPACE
{
    //! Serializes a distance with its canonical units and full double precision.
    void to_json(json& j, const Distance& obj) {
        j = obj.to_parseable_string();
    }
    //! Parses a distance, defaulting to meters; malformed input leaves the destination unchanged.
    void from_json(const json& j, Distance& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::METERS);
        if (v.has_value()) obj = Distance(v->value, v->units);
    }

    //! Serializes an angle with an unambiguous unit token.
    void to_json(json& j, const Angle& obj) {
        j = obj.to_parseable_string();
    }
    //! Parses an angle, defaulting to degrees; malformed input leaves the destination unchanged.
    void from_json(const json& j, Angle& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::DEGREES);
        if (v.has_value()) obj = Angle(v->value, v->units);
    }

    //! Serializes a duration with its canonical units and full double precision.
    void to_json(json& j, const Duration& obj) {
        j = obj.to_parseable_string();
    }
    //! Parses a duration, defaulting to seconds; malformed input leaves the destination unchanged.
    void from_json(const json& j, Duration& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::SECONDS);
        if (v.has_value()) obj = Duration(v->value, v->units);
    }

    //! Serializes a speed with its canonical units and full double precision.
    void to_json(json& j, const Speed& obj) {
        j = obj.to_parseable_string();
    }
    //! Parses a speed, defaulting to meters/second; malformed input leaves the destination unchanged.
    void from_json(const json& j, Speed& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::METERS_PER_SECOND);
        if (v.has_value()) obj = Speed(v->value, v->units);
    }

    //! Serializes a screen size with its canonical units and full double precision.
    void to_json(json& j, const ScreenSize& obj) {
        j = obj.to_parseable_string();
    }
    //! Parses a screen size, defaulting to pixels; malformed input leaves the destination unchanged.
    void from_json(const json& j, ScreenSize& obj) {
        auto v = UnitsParser().parse(get_string(j), Units::PIXELS);
        if (v.has_value()) obj = ScreenSize(v->value, v->units);
    }
}
