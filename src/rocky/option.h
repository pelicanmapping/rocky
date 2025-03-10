/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

namespace ROCKY_NAMESPACE
{
    /**
     * A template for defining an object that is either set or unset and 
     * can have a default value in its unset state.
     * 
     * This differs from std::optional<> by supporting a "default value" in addition
     * to the set/unset flag.
     * 
     * You can default-initialize an optional in a class defition like so.
     * In this example, the "value" property is unset with a default value
     * of 123:
     * 
     * struct MyClass {
     *     option<int> value { 123 };
     *     option<float> fvalue = 123.0f;
     * };
     */
    template<typename T> struct option
    {
        option() : _set(false), _value(T()), _defaultValue(T()) { }
        option(const T& defaultValue) : _set(false), _value(defaultValue), _defaultValue(defaultValue) { }
        option(const T& defaultValue, const T& value) : _set(true), _value(value), _defaultValue(defaultValue) { }
        option(const option<T>& rhs) { operator=(rhs); }
        option<T>& operator =(const option<T>& rhs) { _set=rhs._set; _value=rhs._value; _defaultValue=rhs._defaultValue; return *this; }
        const T& operator =(const T& value) { _set=true; _value=value; return _value; }
        bool operator ==(const option<T>& rhs) const { return _set && rhs._set && _value==rhs._value; }
        bool operator !=(const option<T>& rhs) const { return !( (*this)==rhs); }
        bool operator ==(const T& value) const { return _value==value; }
        bool operator !=(const T& value) const { return _value!=value; }
        bool operator > (const T& value) const { return _value>value; }
        bool operator >=(const T& value) const { return _value>=value; }
        bool operator < (const T& value) const { return _value<value; }
        bool operator <=(const T& value) const { return _value<=value; }
        bool has_value() const { return _set; }
        bool has_value(const T& value) const { return _set && _value == value; } // differs from == in that the value must be explicity set
        const T& value() const { return _value; }
        const T& default_value() const { return _defaultValue; }
        void clear() { _set = false; _value = _defaultValue; }
        const T& value_or(const T& fallback) const { return _set ? _value : fallback; }
        T temp_copy() const { return _value; }

        // gets a mutable reference, automatically setting
        T& mutable_value() { _set = true; return _value; }

        // sets a default value (without altering a set value)
        void set_default(T defValue) { _defaultValue = defValue; if (!_set) _value = defValue; }

        // accessors
        operator const T*() const { return &_value; }
        T* operator ->() { _set=true; return &_value; }
        const T* operator ->() const { return &_value; }
        operator const T& () const { return _set ? _value : _defaultValue; }

    private:
        T _defaultValue;
        T _value;
        bool _set;
        typedef T* option::*unspecified_bool_type;

    public:
        operator unspecified_bool_type() const { return 0; }
    };
}
