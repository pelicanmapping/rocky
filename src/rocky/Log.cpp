/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include <iostream>

using namespace ROCKY_NAMESPACE;

LogStream::LogStream(LogThreshold t) :
    _threshold(t),
    _stream(nullptr)
{
    _null.open("/dev/null", std::ofstream::out | std::ofstream::app);
}

LogStream::LogStream(const LogStream& rhs)
{
    operator=(rhs);
}

LogStream&
LogStream::operator=(const LogStream& rhs)
{
    _threshold = rhs._threshold;
    _stream = rhs._stream;
    _prefix = rhs._prefix;
    _null.open("/dev/null", std::ofstream::out | std::ofstream::app);
    return *this;
}

void
LogStream::attach(std::ostream& value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _stream = &value;
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
std::string Log::prefix = "[rk]";

Log::Log() :
    debug(LogThreshold::DEBUG),
    info(LogThreshold::INFO),
    notice(LogThreshold::NOTICE),
    warn(LogThreshold::WARN)
{
    debug.attach(std::cout);
    debug._prefix = "(debug)";

    info.attach(std::cout);
    info._prefix = "(info)";

    notice.attach(std::cout);

    warn.attach(std::cout);
    warn._prefix = "---WARNING---";
}

Log::Log(const Log& rhs) :
    debug(rhs.debug),
    info(rhs.info),
    notice(rhs.notice),
    warn(rhs.warn)
{
    //nop
}

Log&
Log::operator=(const Log& rhs)
{
    debug = rhs.debug;
    info = rhs.info;
    notice = rhs.notice;
    warn = rhs.warn;
    return *this;
}
