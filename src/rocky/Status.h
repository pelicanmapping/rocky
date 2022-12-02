/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <string>

namespace rocky
{
    /** General purpose status object */
    class ROCKY_EXPORT Status
    {
    public:
        enum Code {
            NoError,
            ResourceUnavailable,  // e.g. failure to access a file, URL, database, or other resource
            ServiceUnavailable,   // e.g. failure to load a plugin, extension, or other module
            ConfigurationError,   // required data or properties missing
            AssertionFailure,     // an illegal software state was detected
            GeneralError          // something else went wrong
        };

    public:
        Status() : _errorCode(NoError) { }
        Status(const Status& rhs) : _errorCode(rhs._errorCode), _errorMsg(rhs._errorMsg) { }
        Status(const Code& code) : _errorCode(code) { }
        Status(const std::string& msg) : _errorCode(GeneralError), _errorMsg(msg) { }
        Status(const Code& code, const std::string& msg) : _errorCode(code), _errorMsg(msg) { }
        bool ok() const { return _errorCode == NoError; }
        bool failed() const { return !ok(); }
        int errorCode() const { return _errorCode; }
        const std::string& message() const { return _errorMsg; }
        bool operator == (const Status& rhs) const { return _errorCode == rhs._errorCode && _errorMsg.compare(rhs._errorMsg) == 0; }
        bool operator != (const Status& rhs) const { return !(*this==rhs); }
        bool const operator ! () const { return failed(); }
        //static Status OK() { return Status(); }
        static Status Error(const Code& code) { return Status(code); }
        static Status Error(const std::string& msg) { return Status(msg); }
        static Status Error(const Code& code, const std::string& msg) { return Status(code, msg); }
        void set(const Code& code, const std::string& msg) { _errorCode = code, _errorMsg = msg; }
        std::string toString() const {
            return _errorCodeText[errorCode() < 6 ? errorCode() : 5] + ": " + message();
        }
    private:
        int _errorCode;
        std::string _errorMsg;
        static std::string _errorCodeText[6];
    };

    extern ROCKY_EXPORT const Status STATUS_OK;

    /**
     * Generic return value that wraps a value type and a Status.
     */
    template<typename T>
    class Result : public Status
    {
    public:
        Result() : Status() { }
        Result(Status::Code code) : Status(code) { }
        Result(const T& val) : _value(val) { }
        Result(const Status& s) : Status(s) { }
        const T& value() const { return _value; }
        T& get() { return _value; }
        const T& get() const { return _value; }
        T* operator -> () { return &_value; }
        const T* operator -> () const { return &_value; }
    private:
        T _value;
    };
}
