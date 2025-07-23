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
#include <unordered_set>

namespace ROCKY_NAMESPACE
{
    //! A callback subscription.
    using CallbackSub = std::shared_ptr<bool>;

    //! Collection of callback subscriptions.
    struct CallbackSubs : public std::unordered_set<CallbackSub> {
        inline CallbackSubs& operator += (CallbackSub sub) {
            this->emplace(sub); return *this;
        }
    };

    template<typename F = void()>
    class Callback
    {
    private:
        using SubRef = std::weak_ptr<bool>;
        using Entry = typename std::pair<SubRef, std::function<F>>;
        mutable std::vector<Entry> entries;
        mutable std::mutex mutex;
        mutable std::atomic_bool firing = { false };

    public:
        //! Adds a callback function, and returns a subscription object.
        //! When the subscription object is destroyed the callback is deactivated.
        [[nodiscard]]
        CallbackSub operator()(std::function<F>&& func) const {
            std::lock_guard<std::mutex> lock(mutex);
            auto sub = CallbackSub(new bool(true));
            entries.emplace_back(SubRef(sub), func);
            return sub;
        }

        //! Remove a callback function associated with a subscription 
        //! returned from operator().
        void remove(CallbackSub sub) const {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
                if (iter->first == sub) {
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