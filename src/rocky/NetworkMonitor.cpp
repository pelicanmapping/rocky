/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "NetworkMonitor.h"

#include <fstream>
#include <iomanip>
#include <sstream>

using namespace ROCKY_NAMESPACE;

namespace
{
    std::string csvQuote(const std::string& value)
    {
        bool needsQuotes = value.find_first_of(",\"\r\n") != std::string::npos;
        if (!needsQuotes)
        {
            return value;
        }

        std::string output;
        output.reserve(value.size() + 2);
        output.push_back('"');
        for (char c : value)
        {
            if (c == '"')
            {
                output.push_back('"');
            }
            output.push_back(c);
        }
        output.push_back('"');
        return output;
    }
}

NetworkMonitor::ScopedRequestLayer::ScopedRequestLayer(std::shared_ptr<NetworkMonitor> monitor, const std::string& layer) :
    ScopedRequestLayer(monitor.get(), layer)
{
    // nop
}

NetworkMonitor::ScopedRequestLayer::ScopedRequestLayer(NetworkMonitor* monitor, const std::string& layer) :
    _monitor(monitor)
{
    if (_monitor)
    {
        _previousLayer = _monitor->getRequestLayer();
        _monitor->setRequestLayer(layer);
    }
}

NetworkMonitor::ScopedRequestLayer::~ScopedRequestLayer()
{
    if (_monitor)
    {
        _monitor->setRequestLayer(_previousLayer);
    }
}

NetworkMonitor::RequestId
NetworkMonitor::begin(const std::string& uri, const std::string& status, const std::string& type)
{
    if (!enabled())
    {
        return 0;
    }

    std::scoped_lock lock(_mutex);

    Request request;
    request.id = _nextRequestId++;
    request.uri = uri;
    request.status = status;
    request.type = type;
    request.layer = _requestLayerByThread[std::this_thread::get_id()];
    request.count = ++_uriCounts[uri];

    _requests[request.id] = request;

    return request.id;
}

void
NetworkMonitor::end(
    RequestId id,
    const std::string& status,
    const std::string& detail,
    std::size_t bytesReceived,
    const std::string& contentType,
    bool fromCache,
    int responseCode)
{
    if (id == 0)
    {
        return;
    }

    std::scoped_lock lock(_mutex);

    auto i = _requests.find(id);
    if (i != _requests.end())
    {
        auto& request = i->second;
        request.status = status;
        request.detail = detail;
        request.bytesReceived = bytesReceived;
        request.contentType = contentType;
        request.fromCache = fromCache;
        request.responseCode = responseCode;
        request.endTime = Clock::now();
        request.complete = true;
    }
}

NetworkMonitor::Requests
NetworkMonitor::requests() const
{
    Requests output;
    getRequests(output);
    return output;
}

void
NetworkMonitor::getRequests(Requests& out) const
{
    std::scoped_lock lock(_mutex);

    out.clear();
    out.reserve(_requests.size());
    for (auto& entry : _requests)
    {
        out.emplace_back(entry.second);
    }
}

bool
NetworkMonitor::enabled() const
{
    return _enabled.load(std::memory_order_relaxed);
}

void
NetworkMonitor::setEnabled(bool value)
{
    _enabled.store(value, std::memory_order_relaxed);
}

void
NetworkMonitor::clear()
{
    std::scoped_lock lock(_mutex);
    _requests.clear();
    _uriCounts.clear();
}

void
NetworkMonitor::setRequestLayer(const std::string& name)
{
    std::scoped_lock lock(_mutex);

    auto threadId = std::this_thread::get_id();
    if (name.empty())
    {
        _requestLayerByThread.erase(threadId);
    }
    else
    {
        _requestLayerByThread[threadId] = name;
    }
}

std::string
NetworkMonitor::getRequestLayer() const
{
    std::scoped_lock lock(_mutex);

    auto i = _requestLayerByThread.find(std::this_thread::get_id());
    return i != _requestLayerByThread.end() ? i->second : std::string();
}

bool
NetworkMonitor::saveCSV(const std::string& filename) const
{
    return saveCSV(requests(), filename);
}

bool
NetworkMonitor::saveCSV(const Requests& requests, const std::string& filename)
{
    std::ofstream out(filename.c_str());
    if (!out)
    {
        return false;
    }

    out << "ID,URI,Duration(ms),Start(ms),End(ms),Layer,Type,Status,Content-Type,Bytes,Count,Response,From Cache,Detail\n";

    if (!requests.empty())
    {
        out << std::fixed << std::setprecision(2);
        auto origin = requests.front().startTime;

        for (auto& request : requests)
        {
            auto endTime = request.complete ? request.endTime : NetworkMonitor::Clock::now();
            auto startMS = std::chrono::duration<double, std::milli>(request.startTime - origin).count();
            auto endMS = std::chrono::duration<double, std::milli>(endTime - origin).count();

            out
                << request.id << ','
                << csvQuote(request.uri) << ','
                << request.elapsedMilliseconds() << ','
                << startMS << ','
                << endMS << ','
                << csvQuote(request.layer) << ','
                << csvQuote(request.type) << ','
                << csvQuote(request.status) << ','
                << csvQuote(request.contentType) << ','
                << request.bytesReceived << ','
                << request.count << ','
                << request.responseCode << ','
                << (request.fromCache ? "true" : "false") << ','
                << csvQuote(request.detail)
                << '\n';
        }
    }

    return true;
}
