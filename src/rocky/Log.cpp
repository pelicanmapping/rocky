/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include <iostream>

using namespace ROCKY_NAMESPACE;

// uncomment to colorize the output levels
//#define USE_ANSI_COLOR_CODES

// statics
LogLevel Log::level = LogLevel::WARN;


#if defined(USE_ANSI_COLOR_CODES) && defined(WIN32)
#include <windows.h>
#endif

namespace
{
    // color codes
    const std::string COLOR_RED = "\033[31m";
    const std::string COLOR_YELLOW = "\033[33m";
    const std::string COLOR_DEFAULT = "\033[39m";

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
        LogStream(LogLevel level, const std::string& prefix, std::ostream& stream, const std::string& colorcode) :
            _level(level),
            _prefix(prefix),
            _stream(&stream),
            _colorcode(colorcode)
        {
            _functionStream._buf._level = level;
            _nullStream.setstate(std::ios_base::badbit);

#if defined(USE_ANSI_COLOR_CODES) && defined(WIN32)
            // enables ANSI escape code processing on windows (on by default in linux)
            auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode;
            if (GetConsoleMode(handle, &mode))
                SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
        }

        // https://stackoverflow.com/questions/8243743/is-there-a-null-stdostream-implementation-in-c-or-libraries
        mutable std::ofstream _nullStream;
        mutable RedirectingStream _functionStream;
        std::ostream* _stream = nullptr;
        LogLevel _level;
        std::string _prefix;
        std::string _colorcode;
    };

    // static streams for each log level
    LogStream g_infoStream(LogLevel::INFO, "[rk-info] ", std::cout, COLOR_DEFAULT);
    LogStream g_warnStream(LogLevel::WARN, "[rk-WARN] ", std::cout, COLOR_YELLOW);
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

#if defined(USE_ANSI_COLOR_CODES)
    output << ls._colorcode;
#endif

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
