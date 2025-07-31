/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IOTypes.h"
#include "Context.h"
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
    return *this;
}

Services::Services()
{
    readImageFromURI = [](const std::string& location, const IOOptions&) { 
        return Failure(Failure::ServiceUnavailable, "Services.readImageFromURI is not implemented"); };

    readImageFromStream = [](std::istream& stream, std::string contentType, const IOOptions& io) {
        return Failure(Failure::ServiceUnavailable, "Services.readImageFromStream is not implemented"); };
}
