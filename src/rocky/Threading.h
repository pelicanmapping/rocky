/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/weejobs.h>
#include <vector>
#include <list>

namespace ROCKY_NAMESPACE
{
    using Cancelable = WEEJOBS_NAMESPACE::cancelable;

    template<class T>
    using Future = WEEJOBS_NAMESPACE::future<T>;

    namespace detail
    {
        //! Sets the name of the current thread
        extern ROCKY_EXPORT void setThreadName(const std::string& name);

        /** Per-thread data store */
        template<class T>
        struct ThreadLocal
        {
            T& value() {
                std::scoped_lock lock(_mutex);
                auto id = std::this_thread::get_id();
                for(auto& p : _data)
                    if (p.first == id)
                        return p.second;
                return _data.emplace_back(id, T()).second;
            }

            void clear() {
                std::scoped_lock lock(_mutex);
                _data.clear();
            }

        private:
            std::mutex _mutex;
            std::list<std::pair<std::thread::id, T>> _data;
        };

        /** Primitive that only allows one thread at a time access to a keyed resourse */
        template<typename T>
        class Gate
        {
        public:
            Gate() = default;

            //! Lock key's gate
            inline void lock(const T& key)
            {
                std::unique_lock<std::mutex> lock(_m);
                for (;;) {
                    if (emplace(key))
                        return;
                    _block.wait(lock);
                }
            }

            //! Unlock the key's gate
            inline void unlock(const T& key)
            {
                std::unique_lock<std::mutex> lock(_m);
                _keys.erase(std::remove(_keys.begin(), _keys.end(), key), _keys.end());
                _block.notify_all();
            }

        private:
            std::mutex _m;
            std::condition_variable_any _block;
            std::vector<T> _keys;

            inline bool emplace(const T& key)
            {
                for(auto& k : _keys)
                    if (k == key)
                        return false;
                _keys.emplace_back(key);
                return true;
            }
        };

        //! Gate the locks for the duration of this object's scope
        template<typename T>
        struct ScopedGate
        {
        public:
            //! Lock a gate based on key "key"
            ScopedGate(Gate<T>& gate, const T& key) :
                _gate(&gate),
                _key(key)
            {
                if (_gate)
                    _gate->lock(key);
            }

            //! Lock a gate based on key "key" IFF the predicate is true,
            //! else it's a nop.
            template<typename CALLABLE>
            ScopedGate(Gate<T>& gate, const T& key, CALLABLE&& pred) :
                _gate(pred() ? &gate : nullptr),
                _key(key)
            {
                if (_gate)
                    _gate->lock(_key);
            }

            //! Lock a gate if the pointer to the gate is valid; else NOOP.
            ScopedGate(std::shared_ptr<Gate<T>>& gate, const T& key) :
                _gate(gate.get()),
                _key(key)
            {
                if (_gate)
                    _gate->lock(_key);
            }

            //! End-of-scope destructor unlocks the gate
            ~ScopedGate()
            {
                if (_gate)
                    _gate->unlock(_key);
            }

        private:
            Gate<T>* _gate = nullptr;
            T _key;
        };
    }

} // namepsace rocky::util

