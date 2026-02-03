/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        /**
         * Tracks usage data by maintaining a sentry-blocked linked list.
         * Each time a something calls "emplace_or_update" the corresponding
         * record moves to the right of the sentry marker. After a cycle you can call
         * flush to process all users that did not call update() in the that cycle
         * and dispose of them.
         */
        template<typename T>
        class SentryTracker
        {
        public:
            struct ListEntry
            {
                T _data;
                typename std::list<ListEntry>::iterator _ptr;
            };

            using List = std::list<ListEntry>;
            using ListIterator = typename List::iterator;

            //! Construct a new tracker.
            SentryTracker()
            {
                reset();
            }

            //! Resets the tracker to its initial state.
            //! Tracked objects are NOT disposed.
            void reset()
            {
                _list.clear();
                _list.emplace_front(); // the sentry marker
                _sentryptr = _list.begin();
                _size = 0;
            }

            List _list;
            ListIterator _sentryptr;
            unsigned _size = 0;

            //! Emplace a new item in the tracker, and receive a token to use
            //! when calling update().
            inline void* emplace(const T& data)
            {
                auto& entry = _list.emplace_front();
                entry._data = data;
                entry._ptr = _list.begin(); // set the token to point to the new entry
                _size++;
                return &entry._ptr;
            }

            //! Inform the tracker that the object associated with the token
            //! is still in use, and return a new token to replace the old one.
            inline void* update(void* token)
            {
                // Move the tracker to the front of the list (ahead of the sentry).
                // Once a cull traversal is complete, all visited tiles will be
                // in front of the sentry, leaving all non-visited tiles behind it.
                auto ptr = static_cast<ListIterator*>(token);
                _list.splice(_list.begin(), _list, *ptr);
                *ptr = _list.begin();
                return ptr;
            }

            //! Calls emplace if token is null, otherwise called update and
            //! returns a new token.
            inline void* emplace_or_update(const T& data, void* token)
            {
                if (token)
                    return update(token);
                else
                    return emplace(data);
            }

            //! Removes any tracked objects that were not updated since the last
            //! flush, and calls dispose() on each one.
            //! @param maxToDispose Maximum number of objects to dispose of in this call
            //! @param minCacheSize Minimum number of objects to keep in the list, even if some of them are expired
            //! @param dispose Function to call when disposing of an object
            template<typename CALLABLE>
            inline void flush(unsigned maxToDispose, unsigned minCacheSize, CALLABLE&& dispose)
            {
                // After cull, all visited tiles are in front of the sentry, and all
                // non-visited tiles are behind it. Start at the sentry position and
                // iterate over the non-visited tiles, checking them for deletion.
                ListIterator i = _sentryptr;
                ListIterator tmp;
                unsigned count = 0;

                for (++i; i != _list.end() && count < maxToDispose && _size > minCacheSize; ++i)
                {
                    ListEntry& le = *i;

                    // user disposal function
                    bool disposed = dispose(le._data);

                    if (disposed)
                    {
                        // back up the iterator so we can safely erase the entry:
                        tmp = i;
                        --i;

                        // remove it from the tracker list:
                        _list.erase(tmp);
                        _size--;
                        ++count;
                    }
                }

                // reset the sentry.
                _list.splice(_list.begin(), _list, _sentryptr);
                _sentryptr = _list.begin();
            }

            //! Snapshot of the object list (for debugging)
            std::vector<T> snapshot() const
            {
                std::vector<T> result;
                result.reserve(_size);
                for (const auto& e : _list)
                    if (e._data)
                        result.emplace_back(e._data);
                return result;
            }
        };
    }
}
