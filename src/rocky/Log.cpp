/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include <iostream>

using namespace ROCKY_NAMESPACE;

// statics
LogLevel Log::level = LogLevel::WARN;

namespace
{
    // global user function.
    LogFunction g_userFunction = nullptr;
    bool g_logUsePrefix = true;

    // stream buffer that redirects chars to a string buffer
    // and then calls the user function whenever that string buffer
    // is flushed.
    struct RedirectingStreamBuf : public std::basic_stringbuf<char, std::char_traits<char>>
    {
        int sync() override {
            if (g_userFunction) {
                std::string s = str();
                if (!s.empty()) {
                    g_userFunction(_level, s);
                    str("");
                }
            }
            return 0;
        }
        LogLevel _level = LogLevel::INFO;
    };

    // stream housing our redirection buffer above
    struct RedirectingStream : public std::basic_ostream<char, std::char_traits<char>>
    {
        RedirectingStreamBuf _buf;
        RedirectingStream() : std::basic_ostream<char, std::char_traits<char>>::basic_ostream(&_buf) { }
    };

    // single-level stream for use by the logger
    struct LogStream
    {
        LogStream(LogLevel level, const std::string& prefix, std::ostream& stream) :
            _level(level),
            _prefix(prefix),
            _stream(&stream)
        {
            _functionStream._buf._level = level;
            _nullStream.setstate(std::ios_base::badbit);
        }

        // https://stackoverflow.com/questions/8243743/is-there-a-null-stdostream-implementation-in-c-or-libraries
        mutable std::ofstream _nullStream;
        mutable RedirectingStream _functionStream;
        std::ostream* _stream = nullptr;
        LogLevel _level;
        std::string _prefix;
    };

    // static streams for each log level
    LogStream g_infoStream(LogLevel::INFO, "[rk-info] ", std::cout);
    LogStream g_warnStream(LogLevel::WARN, "[rk-WARN] ", std::cout);
}

LogFunction&
Log::userFunction()
{
    return g_userFunction;
}

bool&
Log::usePrefix()
{
    return g_logUsePrefix;
}

std::ostream&
Log::log(LogLevel level)
{
    LogStream& ls =
        level == LogLevel::INFO ? g_infoStream :
        g_warnStream;

    auto& output =
        (level > Log::level) ? ls._nullStream :
        (g_userFunction) ? ls._functionStream :
        *ls._stream;

    if (g_logUsePrefix)
        output << ls._prefix;

    return output;
}

std::ostream&
Log::info()
{
    return log(LogLevel::INFO);
}

std::ostream&
Log::warn()
{
    return log(LogLevel::WARN);
}
