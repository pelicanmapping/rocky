/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <iostream>
#include <fstream>
#include <mutex>

namespace ROCKY_NAMESPACE
{
    enum class LogThreshold
    {
        DEBUG, INFO, NOTICE, WARN, SILENT
    };

    class ROCKY_EXPORT LogStream
    {
    public:
        //! output something on the stream if the threshold is met
        template<typename T>
        inline std::ostream& operator << (const T& value) const;

        //! attach an ostream to this logstream
        void attach(std::ostream& value);

    private:
        LogStream(LogThreshold t);
        LogStream(const LogStream& rhs);
        LogStream& operator=(const LogStream& rhs);
        std::ostream& getStream() const;

        LogThreshold _threshold = LogThreshold::NOTICE;
        std::ostream* _stream;
        mutable std::ofstream _null;
        mutable std::mutex _mutex;
        std::string _prefix;

        friend class Log;
    };

    class ROCKY_EXPORT Log
    {
    public:
        LogStream debug;
        LogStream info;
        LogStream notice;
        LogStream warn;

        static LogThreshold threshold;
        static std::string prefix;

        Log();
        Log(const Log& rhs);
        Log& operator=(const Log& rhs);
    };





    // inlines ...

    template<typename T>
    std::ostream& LogStream::operator<<(const T& value) const {
        auto& s = getStream();
        if (&s != &_null && _threshold >= Log::threshold) {
            std::lock_guard<std::mutex> lock(_mutex);
            if (Log::prefix.empty() && _prefix.empty())
                s << value;
            else if (Log::prefix.empty())
                s << _prefix << ' ' << value;
            else if (_prefix.empty())
                s << Log::prefix << ' ' << value;
            else
                s << Log::prefix << _prefix << ' ' << value;
        }
        return s;
    }
}
