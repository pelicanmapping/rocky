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
        SILENT=0, WARN=1, INFO=2
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
    };
}
