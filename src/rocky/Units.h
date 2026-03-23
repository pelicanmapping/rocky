/* rocky
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace ROCKY_NAMESPACE
{
    namespace Units
    {
        enum class Domain
        {
            DISTANCE,
            ANGLE,
            TIME,
            SPEED,
            SCREEN,
            INVALID
        };
    }

    class ROCKY_EXPORT UnitsType
    {
    public:
        inline bool valid() const {
            return
                _type == Units::Domain::SPEED ? (_distance != nullptr && _time != nullptr) :
                _type != Units::Domain::INVALID;
        }

        inline bool canConvert(const UnitsType& to) const;

        inline bool convertTo(const UnitsType& to, double input, double& output) const;

        inline double convertTo(const UnitsType& to, double input) const;

        const std::string& name() const { return _name; }

        const std::string& abbr() const { return _abbr; }

        const Units::Domain& domain() const { return _type; }

        bool operator == (const UnitsType& rhs) const {
            return
                valid() &&
                rhs.valid() &&
                _type == rhs._type &&
                _toBase == rhs._toBase &&
                (_type != Units::Domain::SPEED || *_distance == *rhs._distance) &&
                (_type != Units::Domain::SPEED || *_time == *rhs._time);
        }

        bool operator != (const UnitsType& rhs) const {
            return !operator==(rhs);
        }

        bool isDistance() const { return _type == Units::Domain::DISTANCE; }
        bool isAngle() const { return _type == Units::Domain::ANGLE; }
        bool isTime() const { return _type == Units::Domain::TIME; }
        bool isSpeed() const { return _type == Units::Domain::SPEED; }
        bool isScreenSize() const { return _type == Units::Domain::SCREEN; }

        // Make a new unit definition (LINEAR, ANGULAR, TEMPORAL, SCREEN)
        UnitsType(const char* name, const char* abbr, const Units::Domain& type, double toBase) :
            _name(name),
            _abbr(abbr),
            _type(type),
            _toBase(toBase) {
        }

        // Maks a new unit definition (SPEED)
        UnitsType(const char* name, const char* abbr, const UnitsType& distance, const UnitsType& time) :
            _name(name),
            _abbr(abbr),
            _type(Units::Domain::SPEED),
            _toBase(1.0),
            _distance(&distance),
            _time(&time) {
        }

        UnitsType() {}

        std::string _name;
        std::string _abbr;
        Units::Domain _type = Units::Domain::INVALID;
        double _toBase = 0.0;
        const UnitsType* _distance = nullptr;
        const UnitsType* _time = nullptr;
    };

    struct QualifiedValue
    {
        double value = 0.0;
        UnitsType units;
    };

    class ROCKY_EXPORT UnitsParser
    {
    public:
        UnitsParser();

        std::optional<UnitsType> parseUnits(std::string_view input) const;
        std::optional<QualifiedValue> parse(std::string_view input, const UnitsType& defaultUnits) const;

        int unitTest() const;

    private:
        std::unordered_map<std::string, UnitsType> _table;
        mutable std::mutex _mutex;
    };

    namespace Units
    {
        // Distances; factor converts to METERS:
        const UnitsType CENTIMETERS("centimeters", "cm", Units::Domain::DISTANCE, 0.01);
        const UnitsType FEET("feet", "ft", Units::Domain::DISTANCE, 0.3048);
        const UnitsType FEET_US_SURVEY("feet(us)", "ft", Units::Domain::DISTANCE, 12.0 / 39.37);
        const UnitsType KILOMETERS("kilometers", "km", Units::Domain::DISTANCE, 1000.0);
        const UnitsType METERS("meters", "m", Units::Domain::DISTANCE, 1.0);
        const UnitsType MILES("miles", "mi", Units::Domain::DISTANCE, 1609.334);
        const UnitsType MILLIMETERS("millimeters", "mm", Units::Domain::DISTANCE, 0.001);

        const UnitsType YARDS("yards", "yd", Units::Domain::DISTANCE, 0.9144);
        const UnitsType NAUTICAL_MILES("nautical miles", "nm", Units::Domain::DISTANCE, 1852.0);
        const UnitsType DATA_MILES("data miles", "dm", Units::Domain::DISTANCE, 1828.8);
        const UnitsType INCHES("inches", "in", Units::Domain::DISTANCE, 0.0254);
        const UnitsType FATHOMS("fathoms", "fm", Units::Domain::DISTANCE, 1.8288);
        const UnitsType KILOFEET("kilofeet", "kf", Units::Domain::DISTANCE, 304.8);
        const UnitsType KILOYARDS("kiloyards", "kyd", Units::Domain::DISTANCE, 914.4);

        // Factor converts unit into RADIANS:
        const UnitsType DEGREES("degrees", "\xb0", Units::Domain::ANGLE, 0.017453292519943295);
        const UnitsType RADIANS("radians", "rad", Units::Domain::ANGLE, 1.0);
        const UnitsType BAM("BAM", "bam", Units::Domain::ANGLE, 6.283185307179586476925286766559);
        const UnitsType NATO_MILS("mils", "mil", Units::Domain::ANGLE, 9.8174770424681038701957605727484e-4);
        const UnitsType DECIMAL_HOURS("hours", "h", Units::Domain::ANGLE, 15.0 * 0.017453292519943295);

        // Factor convert unit into SECONDS:
        const UnitsType DAYS("days", "d", Units::Domain::TIME, 86400.0);
        const UnitsType HOURS("hours", "hr", Units::Domain::TIME, 3600.0);
        const UnitsType MICROSECONDS("microseconds", "us", Units::Domain::TIME, 0.000001);
        const UnitsType MILLISECONDS("milliseconds", "ms", Units::Domain::TIME, 0.001);
        const UnitsType MINUTES("minutes", "min", Units::Domain::TIME, 60.0);
        const UnitsType SECONDS("seconds", "s", Units::Domain::TIME, 1.0);
        const UnitsType WEEKS("weeks", "wk", Units::Domain::TIME, 604800.0);

        const UnitsType FEET_PER_SECOND("feet per second", "ft/s", Units::FEET, Units::SECONDS);
        const UnitsType YARDS_PER_SECOND("yards per second", "yd/s", Units::YARDS, Units::SECONDS);
        const UnitsType METERS_PER_SECOND("meters per second", "m/s", Units::METERS, Units::SECONDS);
        const UnitsType KILOMETERS_PER_SECOND("kilometers per second", "km/s", Units::KILOMETERS, Units::SECONDS);
        const UnitsType KILOMETERS_PER_HOUR("kilometers per hour", "kmh", Units::KILOMETERS, Units::HOURS);
        const UnitsType MILES_PER_HOUR("miles per hour", "mph", Units::MILES, Units::HOURS);
        const UnitsType DATA_MILES_PER_HOUR("data miles per hour", "dm/h", Units::DATA_MILES, Units::HOURS);
        const UnitsType KNOTS("nautical miles per hour", "kts", Units::NAUTICAL_MILES, Units::HOURS);

        const UnitsType PIXELS("pixels", "px", Units::Domain::SCREEN, 1.0);

        inline bool canConvert(const UnitsType& from, const UnitsType& to) {
            return from.domain() == to.domain();
        }

        inline void convertSimple(const UnitsType& from, const UnitsType& to, double input, double& output) {
            output = input * from._toBase / to._toBase;
        }

        inline void convertSpeed(const UnitsType& from, const UnitsType& to, double input, double& output) {
            double t = from._distance->convertTo(*to._distance, input);
            output = to._time->convertTo(*from._time, t);
        }

        inline bool convert(const UnitsType& from, const UnitsType& to, double input, double& output) {
            if (canConvert(from, to)) {
                if (from.isDistance() || from.isAngle() || from.isTime())
                    convertSimple(from, to, input, output);
                else if (from.isSpeed())
                    convertSpeed(from, to, input, output);
                return true;
            }
            return false;
        }

        inline double convert(const UnitsType& from, const UnitsType& to, double input) {
            double output = input;
            convert(from, to, input, output);
            return output;
        }

        //extern ROCKY_EXPORT void registerAll(UnitsRepo& repo);

        //extern ROCKY_EXPORT int unitTest(const UnitsRepo& repo);
    }

    namespace detail
    {
        template<typename T> class qualified_double
        {
        public:
            qualified_double(double value, const UnitsType& units) : _value(value), _units(units) {}

            qualified_double(const T& rhs) : _value(rhs._value), _units(rhs._units) {}

            void set(double value, const UnitsType& units) {
                _value = value;
                _units = units;
            }

            T& operator = (const T& rhs) {
                set(rhs._value, rhs._units);
                return static_cast<T&>(*this);
            }

            T operator + (const T& rhs) const {
                return _units.canConvert(rhs._units) ?
                    T(_value + rhs.as(_units), _units) :
                    T(0, {});
            }

            T operator - (const T& rhs) const {
                return _units.canConvert(rhs._units) ?
                    T(_value - rhs.as(_units), _units) :
                    T(0, {});
            }

            T operator * (double rhs) const {
                return T(_value * rhs, _units);
            }

            T operator / (double rhs) const {
                return T(_value / rhs, _units);
            }

            bool operator == (const T& rhs) const {
                return _units.canConvert(rhs._units) && rhs.as(_units) == _value;
            }

            bool operator != (const T& rhs) const {
                return !_units.canConvert(rhs._units) || rhs.as(_units) != _value;
            }

            bool operator < (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value < rhs.as(_units);
            }

            bool operator <= (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value <= rhs.as(_units);
            }

            bool operator > (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value > rhs.as(_units);
            }

            bool operator >= (const T& rhs) const {
                return _units.canConvert(rhs._units) && _value >= rhs.as(_units);
            }

            double as(const UnitsType& convertTo) const {
                return _units.convertTo(convertTo, _value);
            }

            T to(const UnitsType& convertTo) const {
                return T(as(convertTo), convertTo);
            }

            //! Access the value part directly
            double value() const { return _value; }

            //! Access the units part directly
            const UnitsType& units() const { return _units; }

            std::string to_string() const {
                return std::to_string(_value) + _units.abbr();
            }

            virtual std::string to_parseable_string() const {
                return to_string();
            }

        protected:
            double _value;
            UnitsType _units;
        };
    }

    class Distance : public detail::qualified_double<Distance> {
    public:
        Distance() : detail::qualified_double<Distance>(0, Units::METERS) {}
        Distance(double value) : detail::qualified_double<Distance>(value, Units::METERS) {}
        Distance(double value, const UnitsType& units) : detail::qualified_double<Distance>(value, units) {}
    };

    class Angle : public detail::qualified_double<Angle> {
    public:
        Angle() : detail::qualified_double<Angle>(0, Units::DEGREES) {}
        Angle(double value) : detail::qualified_double<Angle>(value, Units::DEGREES) {}
        Angle(double in_value, const UnitsType& in_units) : detail::qualified_double<Angle>(in_value, in_units) {}
        std::string asParseableString() const {
            if (_units == Units::DEGREES) return std::to_string(_value);
            else return to_string();
        }
    };

    class Duration : public detail::qualified_double<Duration> {
    public:
        Duration() : detail::qualified_double<Duration>(0, Units::SECONDS) {}
        Duration(double value) : detail::qualified_double<Duration>(value, Units::SECONDS) {}
        Duration(double in_value, const UnitsType& in_units) : detail::qualified_double<Duration>(in_value, in_units) {}
    };
    typedef Duration Temporal; // backwards compat

    class Speed : public detail::qualified_double<Speed> {
    public:
        Speed() : detail::qualified_double<Speed>(0, Units::METERS_PER_SECOND) {}
        Speed(double value) : detail::qualified_double<Speed>(value, Units::METERS_PER_SECOND) {}
        Speed(double value, const UnitsType& units) : detail::qualified_double<Speed>(value, units) {}
    };

    class ScreenSize : public detail::qualified_double<ScreenSize> {
    public:
        ScreenSize() : detail::qualified_double<ScreenSize>(0, Units::PIXELS) {}
        ScreenSize(double value) : detail::qualified_double<ScreenSize>(value, Units::PIXELS) {}
        ScreenSize(double value, const UnitsType& units) : detail::qualified_double<ScreenSize>(value, units) {}
    };



    // UnitsType inlines
    inline bool UnitsType::canConvert(const UnitsType& to) const {
        return _type == to._type;
    }

    inline bool UnitsType::convertTo(const UnitsType& to, double input, double& output)  const {
        return Units::convert(*this, to, input, output);
    }

    inline double UnitsType::convertTo(const UnitsType& to, double input) const {
        return Units::convert(*this, to, input);
    }
}
