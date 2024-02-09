/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <unordered_map>

#define WEETHREADS_NAMESPACE jobs
#define WEETHREADS_EXPORT ROCKY_EXPORT
#include <rocky/weethreads.h>

namespace ROCKY_NAMESPACE
{
    using Cancelable = jobs::cancelable;

    namespace util
    {
        //! Gets the unique ID of the running thread.
        extern ROCKY_EXPORT unsigned getCurrentThreadId();

        extern ROCKY_EXPORT void setThreadName(const std::string& name);

        /** Per-thread data store */
        template<class T>
        struct ThreadLocal : public std::mutex
        {
            T& value() {
                std::scoped_lock lock(*this);
                return _data[std::this_thread::get_id()];
            }

            void clear() {
                std::scoped_lock lock(*this);
                _data.clear();
            }

            using container_t = typename std::unordered_map<std::thread::id, T>;
            using iterator = typename container_t::iterator;

            // NB. lock before using these!
            iterator begin() { return _data.begin(); }
            iterator end() { return _data.end(); }

        private:
            container_t _data;
        };

        template<typename T>
        class Gate
        {
        public:
            Gate() { }

            inline void lock(const T& key) {
                std::unique_lock<std::mutex> lock(_m);
                auto thread_id = getCurrentThreadId();
                for (;;) {
                    auto i = _keys.emplace(key, thread_id);
                    if (i.second)
                        return;
                    _unlocked.wait(lock);
                }
            }

            inline void unlock(const T& key) {
                std::unique_lock<std::mutex> lock(_m);
                _keys.erase(key);
                _unlocked.notify_all();
            }

        private:
            std::mutex _m;
            std::condition_variable_any _unlocked;
            std::unordered_map<T, unsigned> _keys;
        };

        //! Gate the locks for the duration of this object's scope
        template<typename T>
        struct ScopedGate
        {
        public:
            //! Lock a gate based on key "key"
            ScopedGate(Gate<T>& gate, const T& key) :
                _gate(gate),
                _key(key),
                _active(true)
            {
                _gate.lock(key);
            }

            //! Lock a gate based on key "key" IFF the predicate is true,
            //! else it's a nop.
            ScopedGate(Gate<T>& gate, const T& key, std::function<bool()> pred) :
                _gate(gate),
                _key(key),
                _active(pred())
            {
                if (_active)
                    _gate.lock(_key);
            }

            //! End-of-scope destructor unlocks the gate
            ~ScopedGate()
            {
                if (_active)
                    _gate.unlock(_key);
            }

        private:
            Gate<T>& _gate;
            T _key;
            bool _active;
        };
    }

} // namepsace rocky::util

