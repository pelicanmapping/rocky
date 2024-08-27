/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IOTypes.h"
#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;


IOOptions::IOOptions(const IOOptions& rhs)
{
    this->operator=(rhs);
}

IOOptions::IOOptions(const IOOptions& rhs, Cancelable& c) :
    IOOptions(rhs)
{
    _cancelable = &c;
}

IOOptions::IOOptions(Cancelable& c) :
    _cancelable(&c)
{
    //nop
}

IOOptions::IOOptions(const std::string& in_referrer) :
    referrer(in_referrer)
{
    //nop
}

IOOptions::IOOptions(const IOOptions& rhs, const std::string& in_referrer) :
    IOOptions(rhs)
{
    referrer = in_referrer;
}

IOOptions&
IOOptions::operator = (const IOOptions& rhs)
{
    services = rhs.services;
    referrer = rhs.referrer;
    maxNetworkAttempts = rhs.maxNetworkAttempts;
    uriGate = rhs.uriGate;
    _cancelable = rhs._cancelable;
    _properties = rhs._properties;
    return *this;
}

Services::Services()
{
    readImageFromURI = [](const std::string& location, const IOOptions&) { return Status_ServiceUnavailable; };
    readImageFromStream = [](std::istream& stream, std::string contentType, const IOOptions& io) { return Status_ServiceUnavailable; };
}
