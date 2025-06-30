/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>

#include <atomic>
#include <mutex>
#include <functional>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * Easy way to add a thread-safe callback to a class.
     * 
     * Developer defines a callback, usually as a class member:
     *   Callback<void(int)> onClick;
     *
     * User adds a callback:
     *   instance->onClick([](int a) { ... });
     *
     * Class fires a callback:
     *   onClick(a);
     */
    template<typename F = void()>
    class Callback
    {
    private:
        using Entry = typename std::pair<int, std::function<F>>;
        mutable int uidgen = 0;
        mutable std::vector<Entry> entries;
        mutable std::mutex mutex;
        mutable std::atomic_bool firing = { false };

    public:
        //! Adds a callback function
        int operator()(std::function<F>&& func) const {
            std::lock_guard<std::mutex> lock(mutex);
            auto uid = ++uidgen;
            entries.emplace_back(uid, func);
            return uid;
        }

        //! Removed a callback function with the UID returned from ()
        void remove(int uid) const {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
                if (iter->first == uid) {
                    entries.erase(iter);
                    break;
                }
            }
        }

        //! Executes all callback functions with the provided args
        template<typename... Args>
        void fire(Args&&... args) const {
            if (firing.exchange(true) == false) {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& e : entries)
                    e.second(args...);
                firing = false;
            }
        }

        //! True if there is at least one registered callback to fire
        operator bool() const
        {
            return !entries.empty();
        }
    };

    using CallbackToken = std::shared_ptr<bool>;

    template<typename F = void()>
    class AutoCallback
    {
    private:
        using TokenRef = std::weak_ptr<bool>;
        using Entry = typename std::pair<TokenRef, std::function<F>>;
        mutable std::vector<Entry> entries;
        mutable std::mutex mutex;
        mutable std::atomic_bool firing = { false };

    public:
        //! Adds a callback function
        CallbackToken operator()(std::function<F>&& func) const {
            std::lock_guard<std::mutex> lock(mutex);
            auto token = CallbackToken(new bool(true));
            entries.emplace_back(TokenRef(token), func);
            return token;
        }

        //! Removed a callback function with the UID returned from ()
        void remove(CallbackToken token) const {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
                if (iter->first == token) {
                    entries.erase(iter);
                    break;
                }
            }
        }

        //! Executes all callback functions with the provided args
        template<typename... Args>
        void fire(Args&&... args) const {
            if (firing.exchange(true) == false) {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& e : entries) {
                    if (auto temp = e.first.lock()) // skip abandoned callbacks
                        e.second(args...);
                }
                firing = false;
            }
        }

        //! True if there is at least one registered callback to fire
        operator bool() const
        {
            return !entries.empty();
        }
    };
}