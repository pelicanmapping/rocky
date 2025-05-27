/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Utils.h>
#include <mutex>
#include <list>
#include <algorithm>

namespace ROCKY_NAMESPACE
{
    namespace util
    {
        /**
        * LRUCache is a thread-safe implementation of a Least Recently Used (LRU) cache.
        * It stores key-value pairs and evicts the least recently used item when the cache reaches its capacity.
        * The cache is protected by a mutex to ensure thread safety.
        * Adapted from https://www.geeksforgeeks.org/lru-cache-implementation
        * Optimized by Copilot
        */
        template<class K, class V>
        class LRUCache
        {
        private:
            mutable std::mutex mutex;
            size_t capacity;
            using E = typename std::pair<K, V>;
            mutable typename std::list<E> cache;
            mutable vector_map<K, typename std::list<E>::iterator> map;

        public:
            mutable int hits = 0;
            mutable int gets = 0;

            //! Constructs an LRUCache with the specified capacity.
            //! \param capacity_ The maximum number of items the cache can hold.
            LRUCache(size_t capacity_ = 32) : capacity(capacity_)
            {
                map._container.reserve(capacity);
            }

            //! Sets the cache capacity and clears all current entries and statistics.
            //! \param value The new maximum number of items the cache can hold.
            inline void setCapacity(size_t value)
            {
                std::scoped_lock L(mutex);
                cache.clear();
                map.clear();
                hits = 0;
                gets = 0;
                capacity = std::max((size_t)0, value);
                map._container.reserve(capacity);
            }

            //! Retrieves the value associated with the given key, if present.
            //! Moves the accessed item to the most recently used position.
            //! \param key The key to look up.
            //! \return An optional containing the value if found, or empty if not found.
            inline std::optional<V> get(const K& key) const
            {
                if (capacity == 0)
                    return {};
                std::scoped_lock L(mutex);
                ++gets;
                auto it = map.find(key);
                if (it == map.end())
                    return {};
                if (it->second != std::prev(cache.end()))
                    cache.splice(cache.end(), cache, it->second);
                ++hits;
                return it->second->second;
            }

            //! Inserts or updates the value for the given key.
            //! If the key already exists, updates its value and moves it to the most recently used position.
            //! If the cache is full, evicts the least recently used item.
            //! \param key The key to insert or update.
            //! \param value The value to associate with the key.
            inline void put(const K& key, const V& value)
            {
                if (capacity == 0)
                    return;
                std::scoped_lock L(mutex);
                auto it = map.find(key);
                if (it != map.end()) {
                    // Update value and move to back
                    it->second->second = value;
                    cache.splice(cache.end(), cache, it->second);
                }
                else {
                    if (cache.size() == capacity) {
                        auto first_key = cache.front().first;
                        cache.pop_front();
                        map.erase(first_key);
                    }
                    cache.emplace_back(key, value);
                    map[key] = std::prev(cache.end());
                }
            }

            //! Clears all entries from the cache and resets statistics.
            inline void clear()
            {
                std::scoped_lock L(mutex);
                cache.clear();
                map.clear();
                gets = 0, hits = 0;
            }
        };
    }
}