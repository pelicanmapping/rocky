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
        // Adapted from https://www.geeksforgeeks.org/lru-cache-implementation
        template<class K, class V>
        class LRUCache
        {
        private:
            mutable std::mutex mutex;
            int capacity;
            using E = typename std::pair<K, V>;
            typename std::list<E> cache;
            vector_map<K, typename std::list<E>::iterator> map;

        public:
            int hits = 0;
            int gets = 0;

            LRUCache(int capacity_ = 32) : capacity(capacity_) { }

            inline void setCapacity(int value)
            {
                std::scoped_lock L(mutex);
                cache.clear();
                map.clear();
                hits = 0;
                gets = 0;
                capacity = std::max(0, value);
            }

            inline V get(const K& key)
            {
                if (capacity == 0)
                    return V();
                std::scoped_lock L(mutex);
                ++gets;
                auto it = map.find(key);
                if (it == map.end())
                    return V();
                cache.splice(cache.end(), cache, it->second);
                ++hits;
                return it->second->second;
            }

            inline void put(const K& key, const V& value)
            {
                if (capacity == 0)
                    return;
                std::scoped_lock L(mutex);
                if (cache.size() == capacity)
                {
                    auto first_key = cache.front().first;
                    cache.pop_front();
                    map.erase(first_key);
                }
                cache.emplace_back(key, value);
                map[key] = --cache.end();
            }
        };
    }
}
