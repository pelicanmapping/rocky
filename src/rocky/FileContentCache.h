/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#pragma once
#include <rocky/IOTypes.h>
#include <filesystem>

namespace ROCKY_NAMESPACE
{
    //! Shared URI cache with an in-memory LRU and persistent, integrity-checked content records.
    //! Configure before use. Thread-safe within one instance; disk failures are misses and warn once.
    //! Disk storage has no automatic eviction or HTTP freshness policy. Only successful content is persisted.
    class ROCKY_EXPORT FileContentCache : public ContentCache
    {
    public:
        //! Uses a private rocky-content-v1 subdirectory; relative paths resolve against the working directory.
        explicit FileContentCache(const std::string& directory, std::size_t memoryEntries = 256);
        //! Returns a complete memory/disk entry, or a miss for missing, malformed or mismatched records.
        std::optional<Result<Content>> get(const std::string& key) override;
        //! Publishes a complete disk record through a temporary file; retains content in memory on disk failure.
        void put(const std::string& key, const Result<Content>& value) override;
        //! Clears memory and this cache's .rkc disk records; leaves unrelated files and directories untouched.
        void clear() override;
        //! Reports the front cache's entry capacity (disk storage is unbounded).
        std::size_t capacity() const override { return _memory.capacity(); }
        //! Reports resident front-cache entries, not disk entries.
        std::size_t size() const override { return _memory.size(); }
        //! Counts hits from either tier.
        std::uint32_t hits() const override { return _hits.load(); }
        //! Counts requests missed by both tiers.
        std::uint32_t misses() const override { return _misses.load(); }
        //! Returns the authored directory for configuration serialization.
        const std::string& directory() const { return _directory; }

    private:
        //! Logs a storage error once per instance; caller holds _mutex.
        void warn(const std::string& message);
        std::string _directory;
        std::filesystem::path _path;
        detail::LRUCache<std::string, Result<Content>> _memory;
        std::mutex _mutex;
        std::atomic_uint32_t _hits{0}, _misses{0};
        bool _warned = false;
    };
}
