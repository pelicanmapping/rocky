/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Threading.h>
#include <unordered_map>

namespace ROCKY_NAMESPACE
{
    /**
     * Easy way to add a thread-safe callback to a class.
     * usage:
     *   Callback<Function> onClick;
     *
     * User adds a callback:
     *   instance->onClick([](...) { respond; });
     *
     * Class fires a callback:
     *   onClick(a, b, ...);
     */
    template<typename F>
    struct Callback {
        mutable std::mutex mutex;
        mutable std::unordered_map<UID, F> functions;

        //! Adds a callback function
        UID operator()(const F& func) const {
            std::scoped_lock lock(mutex);
            auto uid = createUID();
            functions[uid] = func;
            return uid;
        }

        //! Removed a callback function with the UID returned from ()
        void remove(UID uid) {
            std::scoped_lock lock(mutex);
            functions.erase(uid);
        }

        //! Executes all callback functions with the provided args
        template<typename... Args>
        void fire(Args&&... args) const {
            std::scoped_lock lock(mutex);
            for (auto& function : functions)
                function.second(args...);
        }
    };
}