/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include <iostream>

using namespace rocky;

LogStream::LogStream(LogThreshold t) :
    _threshold(t),
    _stream(nullptr)
{
    _null.open("/dev/null", std::ofstream::out | std::ofstream::app);
}

LogStream::LogStream(const LogStream& rhs) :
    _threshold(rhs._threshold),
    _stream(rhs._stream)
{
    _null.open("/dev/null", std::ofstream::out | std::ofstream::app);
}

std::ostream&
LogStream::getStream() const
{
    if (_stream != nullptr && _threshold >= Log::threshold)
        return *_stream;
    else
        return _null;
}

LogThreshold Log::threshold = LogThreshold::NOTICE;

Log::Log() :
    debug(LogThreshold::DEBUG),
    info(LogThreshold::INFO),
    notice(LogThreshold::NOTICE),
    warn(LogThreshold::WARN)
{
    debug.attach(std::cout);
    info.attach(std::cout);
    notice.attach(std::cout);
    warn.attach(std::cout);
}
