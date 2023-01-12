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
    /** General purpose status object */
    class ROCKY_EXPORT Status
    {
    public:
        enum Code {
            NoError,
            ResourceUnavailable,  // e.g. failure to access a file, URL, database, or other resource
            ServiceUnavailable,   // e.g. failure to connect to a site, load a plugin, extension, or other module
            ConfigurationError,   // required data or properties missing
            AssertionFailure,     // an illegal software state was detected
            GeneralError          // something else went wrong
        };

        Code code;
        std::string message;

        Status() : code(NoError) { }
        Status(const Status& rhs) = default;
        explicit Status(const Code& c) : code(c) { }
        explicit Status(const std::string& m) : code(GeneralError), message(m) { }
        explicit Status(const Code& c, const std::string& m) : code(c), message(m) { }
        bool ok() const { return code == NoError; }
        bool failed() const { return !ok(); }
        bool operator == (const Status& rhs) const { return code == rhs.code && message.compare(rhs.message) == 0; }
        bool operator != (const Status& rhs) const { return !(*this==rhs); }
        bool const operator ! () const { return failed(); }
        static Status Error(const Code& c) { return Status(c); }
        static Status Error(const std::string& m) { return Status(m); }
        static Status Error(const Code& c, const std::string& m) { return Status(c, m); }
        std::string toString() const {
            return _errorCodeText[(int)code < 6 ? (int)code : 5] + ": " + message;
        }
    private:
        static std::string _errorCodeText[6];
    };

    extern ROCKY_EXPORT const Status StatusOK;

    /**
     * Generic return value that wraps a value type and a Status.
     */
    template<typename T>
    struct Result
    {
        Result(const T& v) : value(v) { }
        Result(const Status& s) : status(s) { }
        explicit Result() : status(Status::ResourceUnavailable) { }
        explicit Result(const Status::Code& c, const std::string& m) : status(c, m) { }
        T* operator -> () { return &value; }
        const T* operator -> () const { return &value; }

        T value;
        Status status;
    };
}
