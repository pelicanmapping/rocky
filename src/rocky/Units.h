/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT Units
    {
    public:
        // linear
        static const Units CENTIMETERS;
        static const Units DATA_MILES;
        static const Units FATHOMS;
        static const Units FEET;
        static const Units FEET_US_SURVEY;  // http://www.wsdot.wa.gov/reference/metrics/foottometer.htm
        static const Units INCHES;
        static const Units KILOFEET;
        static const Units KILOMETERS;
        static const Units KILOYARDS;
        static const Units METERS;
        static const Units MILES;           // statute miles
        static const Units MILLIMETERS;
        static const Units NAUTICAL_MILES;
        static const Units YARDS;

        // angular
        static const Units BAM;
        static const Units DEGREES;
        static const Units NATO_MILS; // http://www.convertworld.com/en/angle/Mil+(NATO).html
        static const Units RADIANS;
        static const Units DECIMAL_HOURS;

        // temporal
        static const Units DAYS;
        static const Units HOURS;
        static const Units MICROSECONDS;
        static const Units MILLISECONDS;
        static const Units MINUTES;
        static const Units SECONDS;
        static const Units WEEKS;

        // speed
        static const Units FEET_PER_SECOND;
        static const Units YARDS_PER_SECOND;
        static const Units METERS_PER_SECOND;
        static const Units KILOMETERS_PER_SECOND;
        static const Units KILOMETERS_PER_HOUR;
        static const Units MILES_PER_HOUR;
        static const Units DATA_MILES_PER_HOUR;
        static const Units KNOTS;

        // screen
        static const Units PIXELS;

        // unit types.
        enum Type
        { 
            TYPE_LINEAR, 
            TYPE_ANGULAR, 
            TYPE_TEMPORAL, 
            TYPE_SPEED, 
            TYPE_SCREEN_SIZE,
            TYPE_INVALID
        };

    public:

        static bool parse( const std::string& input, Units& output );

        // parses a value+units string (like "15cm" or "24px")
        static bool parse( const std::string& input, double& out_value, Units& out_units, const Units& defaultUnits );
        static bool parse( const std::string& input, float& out_value, Units& out_units, const Units& defaultUnits );

        static bool convert( const Units& from, const Units& to, double input, double& output ) {
            if ( canConvert(from, to) ) {
                if ( from._type == TYPE_LINEAR || from._type == TYPE_ANGULAR || from._type == TYPE_TEMPORAL )
                    convertSimple( from, to, input, output );
                else if ( from._type == TYPE_SPEED )
                    convertSpeed( from, to, input, output );
                return true;
            }
            return false;
        }

        static double convert( const Units& from, const Units& to, double input ) {
            double output = input;
            convert( from, to, input, output );
            return output;
        }

        static bool canConvert( const Units& from, const Units& to ) {
            return from._type == to._type;
        }

        bool canConvert( const Units& to ) const {
            return _type == to._type;
        }

        bool convertTo( const Units& to, double input, double& output )  const {
            return convert( *this, to, input, output );
        }

        double convertTo( const Units& to, double input ) const {
            return convert( *this, to, input );
        }
        
        const std::string& getName() const { return _name; }

        const std::string& getAbbr() const { return _abbr; }

        const Type& getType() const { return _type; }

        bool operator == ( const Units& rhs ) const {
            return _type == rhs._type && _toBase == rhs._toBase; }
        
        bool operator != ( const Units& rhs ) const {
            return _type != rhs._type || _toBase != rhs._toBase; }

        bool isLinear() const { return _type == TYPE_LINEAR; }
        bool isDistance() const { return _type == TYPE_LINEAR; }

        bool isAngular() const { return _type == TYPE_ANGULAR; }
        bool isAngle() const { return _type == TYPE_ANGULAR; }

        bool isTemporal() const { return _type == TYPE_TEMPORAL; }
        bool isTime() const { return _type == TYPE_TEMPORAL; }

        bool isSpeed() const { return _type == TYPE_SPEED; }

        bool isScreenSize() const { return _type == TYPE_SCREEN_SIZE; }

    public:

        // Make a new unit definition (LINEAR, ANGULAR, TEMPORAL, SCREEN)
        Units( const std::string& name, const std::string& abbr, const Type& type, double toBase );

        // Maks a new unit definition (SPEED)
        Units( const std::string& name, const std::string& abbr, const Units& distance, const Units& time );

        Units() : _type(TYPE_INVALID), _toBase(0.0), _distance(0L), _time(0L) { }

    private:

        static void convertSimple( const Units& from, const Units& to, double input, double& output ) {
            output = input * from._toBase / to._toBase;
        }
        static void convertSpeed( const Units& from, const Units& to, double input, double& output ) {
            double t = from._distance->convertTo( *to._distance, input );
            output = to._time->convertTo( *from._time, t );
        }


        std::string _name, _abbr;
        Type _type;
        double _toBase;
        const Units* _distance;
        const Units* _time;
        
        // called by Registry to register system units
        static void registerAll();

    public:

        // returns 0 upon success, error code on failure
        static int unitTest();

    };
    
    namespace detail
    {
        template<typename T> class qualified_double
        {
        public:
            qualified_double(double value, const Units& units) : _value(value), _units(units) { }

            qualified_double(const T& rhs) : _value(rhs._value), _units(rhs._units) { }

            // parses the qualified number from a parseable string (e.g., "123km")
            qualified_double(const std::string& parseable, const Units& defaultUnits) : _value(0.0), _units(defaultUnits) {
                Units::parse(parseable, _value, _units, defaultUnits);
            }

            void set(double value, const Units& units) {
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
                    T(0, Units());
            }

            T operator - (const T& rhs) const {
                return _units.canConvert(rhs._units) ?
                    T(_value - rhs.as(_units), _units) :
                    T(0, Units());
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

            double as(const Units& convertTo) const {
                return _units.convertTo(convertTo, _value);
            }

            T to(const Units& convertTo) const {
                return T(as(convertTo), convertTo);
            }

            //! Access the value part directly
            double value() const { return _value; }

            //! Access the units part directly
            const Units& units() const { return _units; }

            std::string to_string() const {
                return std::to_string(_value) + _units.getAbbr();
            }

            virtual std::string to_parseable_string() const {
                return to_string();
            }

        protected:
            double _value;
            Units  _units;
        };
    }

    class Distance : public detail::qualified_double<Distance> {
    public:
        Distance() : qualified_double<Distance>(0, Units::METERS) { }
        Distance(double value, const Units& units =Units::METERS) : qualified_double<Distance>(value, units) { }
        Distance(const std::string& str, const Units& defaultUnits) : qualified_double<Distance>(str, defaultUnits) { }
    };

    class Angle : public detail::qualified_double<Angle> {
    public:
        Angle() : qualified_double<Angle>(0, Units::DEGREES) { }
        Angle(double value, const Units& units =Units::DEGREES) : qualified_double<Angle>(value, units) { }
        Angle(const std::string& str, const Units& defaultUnits) : qualified_double<Angle>(str, defaultUnits) { }
        std::string asParseableString() const {
            if (_units == Units::DEGREES) return std::to_string(_value);
            else return to_string();
        }
    };

    class Duration : public detail::qualified_double<Duration> {
    public:
        Duration() : qualified_double<Duration>(0, Units::SECONDS) { }
        Duration(double value, const Units& units =Units::SECONDS) : qualified_double<Duration>(value, units) { }
        Duration(const std::string& str, const Units& defaultUnits) : qualified_double<Duration>(str, defaultUnits) { }
    };

    class Speed : public detail::qualified_double<Speed> {
    public:
        Speed() : qualified_double<Speed>(0, Units::METERS_PER_SECOND) { }
        Speed(double value, const Units& units) : qualified_double<Speed>(value, units) { }
        Speed(const std::string& str, const Units& defaultUnits) : qualified_double<Speed>(str, defaultUnits) { }
    };

    class ScreenSize : public detail::qualified_double<ScreenSize> {
    public:
        ScreenSize() : qualified_double<ScreenSize>(0, Units::PIXELS) { }
        ScreenSize(double value, const Units& units =Units::PIXELS) : qualified_double<ScreenSize>(value, units) { }
        ScreenSize(const std::string& str, const Units& defaultUnits) : qualified_double<ScreenSize>(str, defaultUnits) { }
    };
}
