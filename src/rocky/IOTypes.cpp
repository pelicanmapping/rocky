/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IOTypes.h"
#include "FileContentCache.h"
#include "Context.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

IOOptions::IOOptions()
{
    _services = std::make_shared<Services>();
}

Services::Services()
{
    readImageFromURI = [](const std::string& location, const IOOptions&) { 
        return Failure(Failure::ServiceUnavailable, "Services.readImageFromURI is not implemented"); };

    readImageFromStream = [](std::istream& stream, std::string contentType, const IOOptions& io) {
        return Failure(Failure::ServiceUnavailable, "Services.readImageFromStream is not implemented"); };
}

Result<> Services::from_json(std::string_view JSON)
{
    const auto j = parse_json(JSON);
    if (j.status.failed()) return j.status.error();
    if (!j.is_object()) return Failure(Failure::ConfigurationError, "services must be an object");
    if (!j.contains("contentCache")) return ResultVoidOK;
    const auto& c = j.at("contentCache");
    if (!c.is_object() || !c.contains("directory") || !c.at("directory").is_string() ||
        c.at("directory").get<std::string>().empty())
        return Failure(Failure::ConfigurationError, "services.contentCache requires a nonempty directory");
    std::size_t entries = 256;
    if (c.contains("memoryEntries"))
    {
        const auto& v = c.at("memoryEntries");
        if (!v.is_number_integer() || v.get<double>() < 0.0 || v.get<double>() > 1048576.0)
            return Failure(Failure::ConfigurationError, "contentCache.memoryEntries must be an integer from 0 to 1048576");
        entries = v.get<std::size_t>();
    }
    try
    {
        const auto directory = c.at("directory").get<std::string>();
        const auto current = std::dynamic_pointer_cast<FileContentCache>(contentCache);
        if (!current || current->directory() != directory || current->capacity() != entries)
            contentCache = std::make_shared<FileContentCache>(directory, entries);
    }
    catch (const std::exception& e)
    {
        return Failure(Failure::ConfigurationError, e.what());
    }
    return ResultVoidOK;
}

std::string Services::to_json() const
{
    auto j = json::object();
    if (auto cache = std::dynamic_pointer_cast<FileContentCache>(contentCache))
        j["contentCache"] = { { "directory", cache->directory() }, { "memoryEntries", cache->capacity() } };
    return j.dump();
}
