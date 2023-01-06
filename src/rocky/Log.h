/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <iostream>
#include <fstream>

namespace rocky
{
    enum class LogThreshold
    {
        DEBUG, INFO, NOTICE, WARN, SILENT
    };

    class ROCKY_EXPORT LogStream
    {
    public:
        template<typename T>
        std::ostream& operator << (const T& value) const {
            auto& s = getStream();
            if (&s != &_null) s << value;
            return s;
        }
        void attach(std::ostream& value) {
            _stream = &value;
        }

    private:
        LogStream(LogThreshold t);
        LogStream(const LogStream& rhs);
        std::ostream& getStream() const;

        LogThreshold _threshold;
        std::ostream* _stream;
        mutable std::ofstream _null;

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

        Log();
    };
}
