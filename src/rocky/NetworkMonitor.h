/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * Thread-safe request history for URI-backed I/O.
     *
     * This class contains no rendering dependencies. UI layers can periodically
     * call requests() and render the returned snapshot however they like.
     */
    class ROCKY_EXPORT NetworkMonitor
    {
    public:
        using RequestId = std::uint64_t;
        using Clock = std::chrono::steady_clock;

        struct Request
        {
            RequestId id = 0;
            std::string uri;
            std::string layer;
            std::string type;
            std::string status;
            std::string detail;
            std::string contentType;
            Clock::time_point startTime = Clock::now();
            Clock::time_point endTime = startTime;
            std::size_t bytesReceived = 0;
            unsigned count = 0;
            int responseCode = 0;
            bool complete = false;
            bool fromCache = false;

            double elapsedMilliseconds() const
            {
                auto t1 = complete ? endTime : Clock::now();
                return std::chrono::duration<double, std::milli>(t1 - startTime).count();
            }
        };

        using Requests = std::vector<Request>;

        class ROCKY_EXPORT ScopedRequestLayer
        {
        public:
            ScopedRequestLayer(std::shared_ptr<NetworkMonitor> monitor, const std::string& layer);
            ScopedRequestLayer(NetworkMonitor* monitor, const std::string& layer);
            ~ScopedRequestLayer();

            ScopedRequestLayer(const ScopedRequestLayer&) = delete;
            ScopedRequestLayer& operator=(const ScopedRequestLayer&) = delete;

        private:
            NetworkMonitor* _monitor = nullptr;
            std::string _previousLayer;
        };

        RequestId begin(const std::string& uri, const std::string& status, const std::string& type = "");

        void end(
            RequestId id,
            const std::string& status,
            const std::string& detail = {},
            std::size_t bytesReceived = 0,
            const std::string& contentType = {},
            bool fromCache = false,
            int responseCode = 0);

        Requests requests() const;
        void getRequests(Requests& out) const;

        bool enabled() const;
        void setEnabled(bool value);

        void clear();

        void setRequestLayer(const std::string& name);
        std::string getRequestLayer() const;

        bool saveCSV(const std::string& filename) const;
        static bool saveCSV(const Requests& requests, const std::string& filename);

    private:
        mutable std::mutex _mutex;
        std::map<RequestId, Request> _requests;
        std::unordered_map<std::string, unsigned> _uriCounts;
        std::unordered_map<std::thread::id, std::string> _requestLayerByThread;
        std::atomic_bool _enabled = false;
        RequestId _nextRequestId = 1;
    };
}
