/* rocky
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/Units.h>
#include <rocky/SRS.h>
#include <cmath>
#include <limits>
#include <type_traits>

#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>

using namespace ROCKY_NAMESPACE;

static_assert(sizeof(Units) == 1, "A units value must contain only its byte-sized identifier");
static_assert(std::is_trivially_copyable_v<Units>, "Unit identifiers must copy without ownership work");
static_assert(std::is_same_v<UnitsType, Units>, "Keep the legacy type name as an alias");
static_assert(std::is_empty_v<UnitsParser>, "Parsing must not retain per-instance lookup state");
static_assert(std::is_trivially_copyable_v<Distance>, "Quantities must have simple value semantics");
static_assert(!std::is_polymorphic_v<Angle>, "Formatting must not add a vtable to quantities");
static_assert(sizeof(Distance) == sizeof(QualifiedValue), "Quantities must store only a value and units");
static_assert(Units{Units::METERS} == Units::METERS, "Unit identifiers must support constexpr comparison");
static_assert(Units::METERS == Units{Units::METERS}, "Enum identifiers must compare in either order");

namespace
{
    //! Verifies that JSON retains both the exact double value and the original unit identifier.
    template<typename T>
    void checkQuantityJson(const T& input)
    {
        json encoded = input;
        REQUIRE(encoded.is_string());
        auto decoded = encoded.get<T>();
        CHECK(decoded.value() == input.value());
        CHECK(decoded.units() == input.units());
    }
}

//! Verifies that invalid enum values are safe to query and cannot masquerade as convertible units.
TEST_CASE("unit identifiers have safe value semantics", "[units]")
{
    Units unit = Units::METERS;
    CHECK(unit.valid());
    CHECK(unit.name() == "meters");
    CHECK(unit.abbr() == "m");
    CHECK(unit.isDistance());
    CHECK_FALSE(unit.isAngle());
    CHECK(unit != Units::FEET);
    CHECK(Units{} == Units{});

    for (auto type : { Units::INVALID, Units::NUM_TYPES, static_cast<Units::Type>(255) })
    {
        Units invalid(type);
        CHECK_FALSE(invalid.valid());
        CHECK(invalid.domain() == Units::Domain::INVALID);
        CHECK(invalid.name().empty());
        CHECK(invalid.abbr().empty());
        CHECK_FALSE(invalid.canConvert(invalid));
        CHECK_FALSE(Units::canConvert(invalid, Units::METERS));
        CHECK_FALSE(Units::canConvert(Units::METERS, invalid));
        CHECK_FALSE(Units::conversionFactor(invalid, invalid).has_value());
        double output = 123.0;
        CHECK_FALSE(Units::convert(invalid, invalid, 9.0, output));
        CHECK(output == 123.0);
        CHECK(std::isnan(invalid.convertTo(Units::METERS, 9.0)));
    }
}

//! Checks representative physical conversions, speed ratios, screen identity, and explicit failure behavior.
TEST_CASE("unit conversions use a shared scale within each domain", "[units]")
{
    CHECK(Units::convert(Units::KILOMETERS, Units::METERS, 2.5) == Approx(2500.0));
    CHECK(Units::convert(Units::INCHES, Units::CENTIMETERS, 1.0) == Approx(2.54));
    CHECK(Units::convert(Units::FEET, Units::METERS, 1.0) == Approx(0.3048));
    CHECK(Units::convert(Units::FEET_US_SURVEY, Units::METERS, 3937.0) == Approx(1200.0));
    CHECK(Units::convert(Units::FEET, Units::FEET_US_SURVEY, 1.0) == Approx(0.999998).epsilon(1e-12));
    CHECK(Units::convert(Units::DEGREES, Units::RADIANS, 180.0) == Approx(3.141592653589793));
    CHECK(Units::convert(Units::BAM, Units::DEGREES, 1.0) == Approx(360.0));
    CHECK(Units::convert(Units::NATO_MILS, Units::DEGREES, 6400.0) == Approx(360.0));
    CHECK(Units::convert(Units::DECIMAL_HOURS, Units::DEGREES, 24.0) == Approx(360.0));
    CHECK(Units::convert(Units::HOURS, Units::SECONDS, -2.0) == Approx(-7200.0));
    CHECK(Units::convert(Units::WEEKS, Units::DAYS, 2.0) == Approx(14.0));
    CHECK(Units::convert(Units::MICROSECONDS, Units::MILLISECONDS, 1000.0) == Approx(1.0));
    CHECK(Units::convert(Units::KILOMETERS_PER_HOUR, Units::METERS_PER_SECOND, 36.0) == Approx(10.0));
    CHECK(Units::convert(Units::METERS_PER_SECOND, Units::KILOMETERS_PER_HOUR, 10.0) == Approx(36.0));
    CHECK(Units::convert(Units::KNOTS, Units::KILOMETERS_PER_HOUR, 1.0) == Approx(1.852));
    CHECK(Units::convert(Units::FEET_PER_SECOND, Units::METERS_PER_SECOND, 1.0) == Approx(0.3048));
    CHECK(Units::convert(Units::MILES_PER_HOUR, Units::MILES_PER_HOUR, 1.0) == 1.0);

    auto factor = Units::conversionFactor(Units::FEET, Units::METERS);
    REQUIRE(factor.has_value());
    CHECK(3.0 * *factor == Approx(0.9144));

    double output = -1.0;
    REQUIRE(Units::convert(Units::PIXELS, Units::PIXELS, 17.0, output));
    CHECK(output == 17.0);
    CHECK(Units::convert(Units::PIXELS, Units::PIXELS, -0.0) == 0.0);
    CHECK(std::signbit(Units::convert(Units::PIXELS, Units::PIXELS, -0.0)));

    CHECK_FALSE(Units::convert(Units::METERS, Units::DEGREES, 17.0, output));
    CHECK(output == 17.0);
    CHECK_FALSE(Units::canConvert(Units::PIXELS, Units::METERS));
    CHECK_FALSE(Units::conversionFactor(Units::DECIMAL_HOURS, Units::HOURS).has_value());
    CHECK(std::isnan(Units::convert(Units::METERS, Units::SECONDS, 17.0)));
}

//! Ensures every catalog entry has unique parseable metadata and an identity conversion.
TEST_CASE("unit catalog names and abbreviations round trip", "[units]")
{
    UnitsParser parser;
    for (unsigned i = 1; i < Units::NUM_TYPES; ++i)
    {
        Units unit(static_cast<Units::Type>(i));
        INFO(unit.name());
        REQUIRE(unit.valid());
        REQUIRE_FALSE(unit.name().empty());
        REQUIRE_FALSE(unit.abbr().empty());
        auto byName = parser.parseUnits(unit.name());
        auto byAbbr = parser.parseUnits(unit.abbr());
        REQUIRE(byName.has_value());
        REQUIRE(byAbbr.has_value());
        CHECK(*byName == unit);
        CHECK(*byAbbr == unit);
        double output = 0.0;
        REQUIRE(unit.convertTo(unit, 123.0, output));
        CHECK(output == 123.0);
    }
    CHECK(parser.parseUnits("ft") == Units{Units::FEET});
    CHECK(parser.parseUnits("ft(us)") == Units{Units::FEET_US_SURVEY});
    CHECK(parser.parseUnits("ftUS") == Units{Units::FEET_US_SURVEY});
    CHECK(parser.parseUnits("hours") == Units{Units::HOURS});
    CHECK(parser.parseUnits("decimal hours") == Units{Units::DECIMAL_HOURS});
    CHECK(parser.parseUnits("h") == Units{Units::DECIMAL_HOURS});
    CHECK(parser.parseUnits("\xb0") == Units{Units::DEGREES});
    CHECK(parser.parseUnits("\xc2\xb0") == Units{Units::DEGREES});
    CHECK(parser.parseUnits("km/h") == Units{Units::KILOMETERS_PER_HOUR});
    CHECK(parser.parseUnits("knots") == Units{Units::KNOTS});
    CHECK_FALSE(parser.parseUnits("").has_value());
    CHECK_FALSE(parser.parseUnits("unknown").has_value());
}

//! Exercises bounded views, signs, scientific notation, defaults, and rejection of incomplete numeric input.
TEST_CASE("unit parser respects input bounds and rejects malformed numbers", "[units]")
{
    UnitsParser parser;
    for (auto input : { "123e-003m", "123E-003m", "  +.123 meters \t" })
    {
        auto parsed = parser.parse(input, Units::MILES);
        REQUIRE(parsed.has_value());
        CHECK(parsed->value == Approx(0.123));
        CHECK(parsed->units == Units::METERS);
    }
    for (auto input : { "123e+003m", "123E+003m" })
    {
        auto parsed = parser.parse(input, Units::MILES);
        REQUIRE(parsed.has_value());
        CHECK(parsed->value == 123000.0);
        CHECK(parsed->units == Units::METERS);
    }
    auto bare = parser.parse(" -12.5 ", Units::KILOMETERS);
    REQUIRE(bare.has_value());
    CHECK(bare->value == -12.5);
    CHECK(bare->units == Units::KILOMETERS);

    const char number[] = { '1', '2', '3', '4' };
    auto boundedNumber = parser.parse(std::string_view(number, 3), Units::METERS);
    REQUIRE(boundedNumber.has_value());
    CHECK(boundedNumber->value == 123.0);
    const char qualified[] = { '1', '2', 'm', 's' };
    auto boundedUnits = parser.parse(std::string_view(qualified, 3), Units::SECONDS);
    REQUIRE(boundedUnits.has_value());
    CHECK(boundedUnits->units == Units::METERS);
    CHECK(parser.parseUnits(std::string_view(qualified + 2, 1)) == Units{Units::METERS});
    CHECK_FALSE(parser.parse(std::string_view("1m\0junk", 7), Units::METERS).has_value());

    for (auto input : { "", " ", "m", "+", "+-1m", "++1m", "1e", "1e-m", "1unknown", "1m junk",
        "nanm", "infm", "1e999m", "1e-999m" })
    {
        INFO(input);
        CHECK_FALSE(parser.parse(input, Units::METERS).has_value());
    }
    CHECK_FALSE(parser.parse("12", Units{}).has_value());
    CHECK(parser.parse("12m", Units{}).has_value());
}

//! Guards quantity arithmetic, retained display units, and invalid conversion results after removing virtual state.
TEST_CASE("quantities retain value semantics and conversion behavior", "[units]")
{
    CHECK(Distance().units() == Units::METERS);
    CHECK(Angle().units() == Units::DEGREES);
    CHECK(Duration().units() == Units::SECONDS);
    CHECK(Speed().units() == Units::METERS_PER_SECOND);
    CHECK(ScreenSize().units() == Units::PIXELS);

    Distance km(1.0, Units::KILOMETERS);
    auto total = km + Distance(500.0);
    CHECK(total.value() == Approx(1.5));
    CHECK(total.units() == Units::KILOMETERS);
    CHECK((km - Distance(500.0)).value() == Approx(0.5));
    CHECK((km * 2.0).value() == 2.0);
    CHECK((km / 2.0).value() == 0.5);
    CHECK(km == Distance(1000.0));
    CHECK(km > Distance(500.0));
    CHECK(km.to(Units::METERS).value() == 1000.0);
    Distance copy = km;
    copy.set(3.0, Units::FEET);
    CHECK(km.value() == 1.0);
    CHECK(copy.as(Units::METERS) == Approx(0.9144));
    CHECK_FALSE((km + Distance(1.0, Units::DEGREES)).units().valid());
    CHECK(std::isnan(km.as(Units::SECONDS)));
    auto invalid = km.to(Units::SECONDS);
    CHECK_FALSE(invalid.units().valid());
    CHECK(std::isnan(invalid.value()));
    CHECK(ScreenSize(7.0).as(Units::PIXELS) == 7.0);
}

//! Verifies exact numeric and unit preservation across serialization, including extreme finite double values.
TEST_CASE("quantity text and JSON serialization preserve precision and units", "[units]")
{
    UnitsParser parser;
    for (unsigned i = 1; i < Units::NUM_TYPES; ++i)
    {
        Units unit(static_cast<Units::Type>(i));
        for (double value : { 0.0, -0.0, 1.2345678901234567, -123456789012345.67,
            std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
            std::numeric_limits<double>::denorm_min() })
        {
            INFO(unit.name());
            Distance original(value, unit);
            auto parsed = parser.parse(original.to_parseable_string(), Units{});
            REQUIRE(parsed.has_value());
            CHECK(parsed->value == value);
            CHECK(std::signbit(parsed->value) == std::signbit(value));
            CHECK(parsed->units == unit);
        }
    }
    checkQuantityJson(Distance(1.2345678901234567, Units::FEET_US_SURVEY));
    checkQuantityJson(Angle(15.123456789012345, Units::DEGREES));
    checkQuantityJson(Angle(2.5, Units::DECIMAL_HOURS));
    checkQuantityJson(Duration(15.123456789012345, Units::HOURS));
    checkQuantityJson(Speed(15.123456789012345, Units::KNOTS));
    checkQuantityJson(ScreenSize(15.123456789012345));
    CHECK(Angle(90).to_parseable_string() == "90deg");
    CHECK(Distance(3, Units::FEET_US_SURVEY).to_parseable_string() == "3ft(us)");
    CHECK(Distance(1, Units{}).to_parseable_string().empty());
    CHECK(Distance(std::numeric_limits<double>::infinity()).to_parseable_string().empty());
    CHECK(Distance(std::numeric_limits<double>::quiet_NaN()).to_parseable_string().empty());

    Distance target(3, Units::FEET);
    json malformed = "not a distance";
    malformed.get_to(target);
    CHECK(target.value() == 3.0);
    CHECK(target.units() == Units::FEET);
    json legacyDegrees = "90\xb0";
    CHECK(legacyDegrees.get<Angle>().value() == 90.0);
    CHECK(json("1.5kilometers").get<Distance>().as(Units::METERS) == 1500.0);
}

//! Keeps angular distances dependent on latitude and ellipsoid rather than treating angles as linear units.
TEST_CASE("angular distance conversion retains SRS context", "[units][srs]")
{
    Distance angular(1.0, Units::DEGREES);
    CHECK(std::isnan(angular.as(Units::METERS)));
    const auto& srs = SRS::WGS84;
    double equator = srs.transformDistance(angular, Units::METERS, Angle(0));
    double latitude60 = srs.transformDistance(angular, Units::METERS, Angle(60));
    CHECK(equator == Approx(srs.ellipsoid().longitudinalDegreesToMeters(1.0, 0.0)));
    CHECK(latitude60 == Approx(srs.ellipsoid().longitudinalDegreesToMeters(1.0, 60.0)));
    CHECK(latitude60 < equator);
    CHECK(srs.transformDistance(Distance(latitude60), Units::DEGREES, Angle(60)) == Approx(1.0));
    CHECK(srs.transformDistance(angular, Units::RADIANS) == Approx(3.141592653589793 / 180.0));
}
