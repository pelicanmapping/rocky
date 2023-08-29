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


#if defined(USE_ANSI_COLOR_CODES) && defined(WIN32)
#include <windows.h>
#endif

// stream buffer that redirects chars to a string buffer
// and then calls the user function whenever that string buffer
// is flushed.
int Log::RedirectingStreamBuf::sync()
{
    if (g_userFunction) {
        std::string s = str();
        if (!s.empty()) {
            g_userFunction(_level, s);
            str("");
        }
    }
    return 0;
}

// stream housing our redirection buffer above
Log::RedirectingStream::RedirectingStream() :
    std::basic_ostream<char, std::char_traits<char>>::basic_ostream(&_buf)
{
    //nop
}

// single-level stream for use by the logger
Log::LogStream::LogStream(LogLevel level, const std::string& prefix, std::ostream& stream, const std::string& colorcode) :
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
