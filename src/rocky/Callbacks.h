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
    using CallbackSubscription = std::shared_ptr<bool>;

    //! Collection of callback subscriptions.
    struct CallbackSubscriptions : public std::unordered_set<CallbackSubscription> {
        inline CallbackSubscriptions& operator += (CallbackSubscription sub) {
            this->emplace(sub); return *this;
        }
    };

    // backwards compat
    using CallbackSub = CallbackSubscription;
    using CallbackSubs = CallbackSubscriptions;

    template<typename... Args>
    class Callback
    {
    public:
        using FuncType = typename std::function<void(Args...)>;

    private:
        using SubRef = std::weak_ptr<bool>;
        using Entry = typename std::pair<SubRef, FuncType>;
        mutable std::vector<Entry> entries;
        mutable std::mutex mutex;
        mutable std::atomic_bool firing = { false };

    public:
        //! Adds a callback function, and returns a subscription object.
        //! When the subscription object is destroyed the callback is deactivated.
        [[nodiscard]]
        CallbackSubscription operator()(FuncType&& func) const {
            std::lock_guard<std::mutex> lock(mutex);
            auto sub = CallbackSubscription(new bool(true));
            entries.emplace_back(SubRef(sub), func);
            return sub;
        }

        //! Remove a callback function associated with a subscription 
        //! returned from operator().
        void remove(CallbackSubscription sub) const {
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