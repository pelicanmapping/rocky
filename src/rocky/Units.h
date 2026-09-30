/* rocky
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ROCKY_NAMESPACE
{
    //! A small unit identifier. Definitions are immutable and shared by all instances.
    struct ROCKY_EXPORT Units
    {
        enum Type : std::uint8_t
        {
            INVALID,
            CENTIMETERS, FEET, FEET_US_SURVEY, KILOMETERS, METERS, MILES, MILLIMETERS,
            YARDS, NAUTICAL_MILES, DATA_MILES, INCHES, FATHOMS, KILOFEET, KILOYARDS,
            DEGREES, RADIANS, BAM, NATO_MILS, DECIMAL_HOURS,
            DAYS, HOURS, MICROSECONDS, MILLISECONDS, MINUTES, SECONDS, WEEKS,
            FEET_PER_SECOND, YARDS_PER_SECOND, METERS_PER_SECOND, KILOMETERS_PER_SECOND,
            KILOMETERS_PER_HOUR, MILES_PER_HOUR, DATA_MILES_PER_HOUR, KNOTS,
            PIXELS,
            NUM_TYPES
        };

        enum class Domain : std::uint8_t
        {
            DISTANCE, ANGLE, TIME, SPEED, SCREEN, INVALID
        };

        Type type;

        //! Creates an identifier, defaulting to invalid; does not allocate or own metadata.
        constexpr Units(Type value = INVALID) noexcept : type(value) {}

        //! Reports whether this identifier names a supported unit.
        constexpr bool valid() const noexcept { return type > INVALID && type < NUM_TYPES; }

        //! Compares identifiers, including invalid identifiers, without comparing conversion scales.
        friend constexpr bool operator == (Units lhs, Units rhs) noexcept { return lhs.type == rhs.type; }

        //! Reports whether two identifiers differ.
        friend constexpr bool operator != (Units lhs, Units rhs) noexcept { return !(lhs == rhs); }

        //! Returns program-lifetime metadata, or an empty view for an invalid identifier.
        std::string_view name() const noexcept;

        //! Returns the unique, parseable abbreviation, or an empty view for an invalid identifier.
        std::string_view abbr() const noexcept;

        //! Returns the conversion domain, or INVALID for an unsupported identifier.
        Domain domain() const noexcept;

        //! Reports whether the unit measures a linear distance.
        bool isDistance() const noexcept { return domain() == Domain::DISTANCE; }

        //! Reports whether the unit measures an angle.
        bool isAngle() const noexcept { return domain() == Domain::ANGLE; }

        //! Reports whether the unit measures elapsed time.
        bool isTime() const noexcept { return domain() == Domain::TIME; }

        //! Reports whether the unit measures distance per time.
        bool isSpeed() const noexcept { return domain() == Domain::SPEED; }

        //! Reports whether the unit measures screen size.
        bool isScreenSize() const noexcept { return domain() == Domain::SCREEN; }

        //! Reports whether both identifiers are valid and share a conversion domain.
        static bool canConvert(Units from, Units to) noexcept;

        //! Returns a reusable multiplier, or nullopt for invalid or incompatible units.
        static std::optional<double> conversionFactor(Units from, Units to) noexcept;

        //! Converts within a domain; failure leaves output unchanged. IEEE nonfinite inputs propagate.
        static bool convert(Units from, Units to, double input, double& output) noexcept;

        //! Converts within a domain, returning NaN for invalid or incompatible units.
        static double convert(Units from, Units to, double input) noexcept;

        //! Reports whether this unit can convert to the target without geospatial context.
        bool canConvert(Units to) const noexcept { return canConvert(*this, to); }

        //! Converts from this unit; failure leaves output unchanged.
        bool convertTo(Units to, double input, double& output) const noexcept {
            return convert(*this, to, input, output);
        }

        //! Converts from this unit, returning NaN for invalid or incompatible units.
        double convertTo(Units to, double input) const noexcept { return convert(*this, to, input); }
    };

    using UnitsType = Units; // Source compatibility with the former descriptor type.

    struct QualifiedValue
    {
        double value = 0.0;
        Units units;
    };

    //! Stateless parser; instances may be shared between threads without synchronization.
    class ROCKY_EXPORT UnitsParser
    {
    public:
        //! Resolves a name or abbreviation after trimming ASCII whitespace; unknown names return nullopt.
        std::optional<Units> parseUnits(std::string_view input) const;

        //! Parses a finite number and optional units within the view's bounds; malformed input returns nullopt.
        //! Missing units use defaultUnits, which must be valid in that case.
        std::optional<QualifiedValue> parse(std::string_view input, Units defaultUnits) const;
    };

    namespace detail
    {
        //! Formats a finite value and canonical units for lossless parsing; invalid input returns an empty string.
        ROCKY_EXPORT std::string formatQualifiedValue(double value, Units units);

        //! Value semantics shared by quantities; stores only the original value and its unit identifier.
        template<typename T> class qualified_double
        {
        public:
            //! Retains the supplied value and units without imposing a domain restriction.
            qualified_double(double value, Units units) : _value(value), _units(units) {}

            //! Replaces the value and its unit identifier.
            void set(double value, Units units) {
                _value = value;
                _units = units;
            }

            //! Adds compatible quantities in this quantity's units; incompatible units yield an invalid quantity.
            T operator + (const T& rhs) const {
                return _units.canConvert(rhs._units) ? T(_value + rhs.as(_units), _units) : T(0, {});
            }

            //! Subtracts compatible quantities in this quantity's units; incompatible units yield an invalid quantity.
            T operator - (const T& rhs) const {
                return _units.canConvert(rhs._units) ? T(_value - rhs.as(_units), _units) : T(0, {});
            }

            //! Scales the value while retaining its units.
            T operator * (double rhs) const { return T(_value * rhs, _units); }

            //! Divides the value while retaining its units; IEEE division-by-zero behavior applies.
            T operator / (double rhs) const { return T(_value / rhs, _units); }

            //! Compares values after conversion; incompatible quantities are unequal.
            bool operator == (const T& rhs) const {
                return _units.canConvert(rhs._units) && rhs.as(_units) == _value;
            }

            //! Reports inequality, including quantities with incompatible units.
            bool operator != (const T& rhs) const {
                return !_units.canConvert(rhs._units) || rhs.as(_units) != _value;
            }

            //! Compares compatible quantities; incompatible units are unordered.
            bool operator < (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value < rhs.as(_units);
            }

            //! Compares compatible quantities; incompatible units are unordered.
            bool operator <= (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value <= rhs.as(_units);
            }

            //! Compares compatible quantities; incompatible units are unordered.
            bool operator > (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value > rhs.as(_units);
            }

            //! Compares compatible quantities; incompatible units are unordered.
            bool operator >= (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value >= rhs.as(_units);
            }

            //! Returns the converted value, or NaN for invalid or incompatible units.
            double as(Units convertTo) const { return Units::convert(_units, convertTo, _value); }

            //! Returns a converted quantity; incompatible units yield an invalid quantity with a NaN value.
            T to(Units convertTo) const {
                return T(as(convertTo), _units.canConvert(convertTo) ? convertTo : Units{});
            }

            //! Accesses the value without conversion.
            double value() const { return _value; }

            //! Returns the unit identifier by value.
            Units units() const { return _units; }

            //! Formats a display value using the canonical abbreviation.
            std::string to_string() const { return std::to_string(_value) + std::string(_units.abbr()); }

            //! Serializes the value and units with enough precision to round-trip a double.
            std::string to_parseable_string() const { return formatQualifiedValue(_value, _units); }

        protected:
            double _value;
            Units _units;
        };
    }

    //! A distance, including angular extents interpreted through an SRS when conversion needs an ellipsoid.
    class Distance : public detail::qualified_double<Distance> {
    public:
        //! Creates a zero distance in meters.
        Distance() : detail::qualified_double<Distance>(0, Units::METERS) {}
        //! Creates a distance in meters.
        Distance(double value) : detail::qualified_double<Distance>(value, Units::METERS) {}
        //! Retains a distance and its supplied units, including angular units.
        Distance(double value, Units units) : detail::qualified_double<Distance>(value, units) {}
    };

    class Angle : public detail::qualified_double<Angle> {
    public:
        //! Creates a zero angle in degrees.
        Angle() : detail::qualified_double<Angle>(0, Units::DEGREES) {}
        //! Creates an angle in degrees.
        Angle(double value) : detail::qualified_double<Angle>(value, Units::DEGREES) {}
        //! Retains an angle and its supplied units.
        Angle(double value, Units units) : detail::qualified_double<Angle>(value, units) {}
        //! Preserves the legacy spelling for callers that serialize angles explicitly.
        std::string asParseableString() const { return to_parseable_string(); }
    };

    class Duration : public detail::qualified_double<Duration> {
    public:
        //! Creates a zero duration in seconds.
        Duration() : detail::qualified_double<Duration>(0, Units::SECONDS) {}
        //! Creates a duration in seconds.
        Duration(double value) : detail::qualified_double<Duration>(value, Units::SECONDS) {}
        //! Retains a duration and its supplied units.
        Duration(double value, Units units) : detail::qualified_double<Duration>(value, units) {}
    };
    using Temporal = Duration; // Source compatibility.

    class Speed : public detail::qualified_double<Speed> {
    public:
        //! Creates a zero speed in meters per second.
        Speed() : detail::qualified_double<Speed>(0, Units::METERS_PER_SECOND) {}
        //! Creates a speed in meters per second.
        Speed(double value) : detail::qualified_double<Speed>(value, Units::METERS_PER_SECOND) {}
        //! Retains a speed and its supplied units.
        Speed(double value, Units units) : detail::qualified_double<Speed>(value, units) {}
    };

    class ScreenSize : public detail::qualified_double<ScreenSize> {
    public:
        //! Creates a zero screen size in pixels.
        ScreenSize() : detail::qualified_double<ScreenSize>(0, Units::PIXELS) {}
        //! Creates a screen size in pixels.
        ScreenSize(double value) : detail::qualified_double<ScreenSize>(value, Units::PIXELS) {}
        //! Retains a screen size and its supplied units.
        ScreenSize(double value, Units units) : detail::qualified_double<ScreenSize>(value, units) {}
    };
}
