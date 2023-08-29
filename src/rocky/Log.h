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
#include <functional>
#include <strstream>
#include <sstream>

namespace ROCKY_NAMESPACE
{
    //! Threshold for logging
    enum class LogLevel
    {
        SILENT=0, WARN=1, INFO=2, ALL=3
    };

    //! Function for redirecting log messages from the console
    using LogFunction = std::function<void(LogLevel, const std::string&)>;

    /**
     * Logging interface (static)
     * Usage:
     *    Log::info() << "Hello, world." << std::endl;
     */
    class ROCKY_EXPORT Log
    {
    public:
        static std::ostream& log(LogLevel level);
        static std::ostream& info();
        static std::ostream& warn();

        //! ignore messages above this level (defaults to INFO)
        static LogLevel level;

        //! redirect all logging to a user function
        static LogFunction& userFunction();

        //! Toggle whether to prepend a metadata prefix to each log entry,
        //! e.g. "[rk-info]"
        static bool& usePrefix();

        Log() = delete;

    public:
        struct RedirectingStreamBuf : public std::basic_stringbuf<char, std::char_traits<char>> {
            LogLevel _level = LogLevel::INFO;
            int sync() override;
        };
        struct RedirectingStream : public std::basic_ostream<char, std::char_traits<char>> {
            RedirectingStreamBuf _buf;
            RedirectingStream();
        };
        struct LogStream {
            LogStream(LogLevel level, const std::string& prefix, std::ostream& stream, const std::string& colorcode);        // https://stackoverflow.com/questions/8243743/is-there-a-null-stdostream-implementation-in-c-or-libraries
            mutable std::ofstream _nullStream;
            mutable RedirectingStream _functionStream;
            std::ostream* _stream = nullptr;
            LogLevel _level;
            std::string _prefix;
            std::string _colorcode;
        };

    private:
        static LogFunction g_userFunction;
        static bool g_logUsePrefix;
        static LogStream g_infoStream;
        static LogStream g_warnStream;
    };
}
