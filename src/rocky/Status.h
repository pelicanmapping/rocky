/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <string>

namespace ROCKY_NAMESPACE
{
    const std::string StatusText[] = {
        "OK",
        "Resource unavailable",
        "Service unavailable",
        "Configuration error",
        "Assertion failure",
        "General error"
    };

    /** General purpose status object - header only */
    struct Status
    {
        enum Code {
            NoError,
            ResourceUnavailable,  // e.g. failure to access a file, URL, database, or other resource
            ServiceUnavailable,   // e.g. failure to connect to a site, load a plugin, extension, or other module
            ConfigurationError,   // required data or properties missing
            AssertionFailure,     // an illegal software state was detected
            GeneralError          // something else went wrong
        };

        Code code = NoError;
        std::string message = {};

        Status() = default;
        Status(const Status& rhs) : code(rhs.code), message(rhs.message) { }
        Status& operator=(const Status& rhs) { code = rhs.code, message = rhs.message; return *this; }
        explicit Status(const Code& c) : code(c) { }
        explicit Status(const std::string& m) : code(GeneralError), message(m) { }
        explicit Status(const Code& c, const std::string& m) : code(c), message(m) { }
        bool ok() const { return code == NoError; }
        bool failed() const { return !ok(); }
        bool operator == (const Status& rhs) const { return code == rhs.code && message.compare(rhs.message) == 0; }
        bool operator != (const Status& rhs) const { return !(*this==rhs); }
        bool const operator ! () const { return failed(); }
        std::string toString() const {
            return message.empty() ? StatusText[(int)code < 6 ? (int)code : 5] :
                StatusText[(int)code < 6 ? (int)code : 5] + ": " + message;
        }
    };

    //! Global convenience objects
    const Status Status_OK, StatusOK;
    const Status Status_ServiceUnavailable(Status::ServiceUnavailable);
    const Status Status_ResourceUnavailable(Status::ResourceUnavailable);
    const Status Status_ConfigurationError(Status::ConfigurationError);
    const Status Status_AssertionFailure(Status::AssertionFailure);
    const Status Status_GeneralError(Status::GeneralError);

    /**
     * Generic return value that wraps a value type and a Status.
     */
    template<typename T>
    struct Result
    {
        T value;
        Status status;

        Result(const T& v) : value(v) { }
        Result(const Status& s) : status(s) { }
        explicit Result() : status(Status::ResourceUnavailable) { }
        explicit Result(const Status::Code& c, const std::string& m) : status(c, m) { }
        T* operator -> () { return &value; }
        const T* operator -> () const { return &value; }
    };
}
