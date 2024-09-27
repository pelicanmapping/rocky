/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>

namespace ROCKY_NAMESPACE
{
    namespace util
    {
        /**
         * Tracks usage data by maintaining a sentry-blocked linked list.
         * Each time a use called "use" the corresponding record moves to
         * the right of the sentry marker. After a cycle you can call
         * collectTrash to process all users that did not call use() in the
         * that cycle, and dispose of them.
         */
        template<typename T>
        class SentryTracker
        {
        public:
            struct ListEntry
            {
                ListEntry(T data, void* token) : _data(data), _token(token) { }
                T _data;
                void* _token;
            };

            using List = std::list<ListEntry>;
            using ListIterator = typename List::iterator;
            using Token = ListIterator;

            SentryTracker()
            {
                reset();
            }

            ~SentryTracker()
            {
                for (auto& e : _list)
                {
                    Token* te = static_cast<Token*>(e._token);
                    if (te)
                        delete te;
                }
            }

            void reset()
            {
                for (auto& e : _list)
                {
                    Token* te = static_cast<Token*>(e._token);
                    if (te)
                        delete te;
                }
                _list.clear();
                _list.emplace_front(nullptr, nullptr); // the sentry marker
                _sentryptr = _list.begin();
            }

            List _list;
            ListIterator _sentryptr;

            inline void* use(const T& data, void* token)
            {
                // Find the tracker for this tile and update its timestamp
                if (token)
                {
                    Token* ptr = static_cast<Token*>(token);

                    // Move the tracker to the front of the list (ahead of the sentry).
                    // Once a cull traversal is complete, all visited tiles will be
                    // in front of the sentry, leaving all non-visited tiles behind it.
                    _list.splice(_list.begin(), _list, *ptr);
                    *ptr = _list.begin();
                    return ptr;
                }
                else
                {
                    // New entry:
                    Token* ptr = new Token();
                    _list.emplace_front(data, ptr); // ListEntry
                    *ptr = _list.begin();
                    return ptr;
                }
            }

            template<typename CALLABLE>
            inline void flush(unsigned maxCount, CALLABLE&& dispose) //std::function<bool(T& obj)> dispose)
            {
                // After cull, all visited tiles are in front of the sentry, and all
                // non-visited tiles are behind it. Start at the sentry position and
                // iterate over the non-visited tiles, checking them for deletion.
                ListIterator i = _sentryptr;
                ListIterator tmp;
                unsigned count = 0;

                for (++i; i != _list.end() && count < maxCount; ++i)
                {
                    ListEntry& le = *i;

                    bool disposed = true;

                    // user disposal function
                    //if (dispose != nullptr)
                    disposed = dispose(le._data);

                    if (disposed)
                    {
                        // back up the iterator so we can safely erase the entry:
                        tmp = i;
                        --i;

                        // delete the token
                        delete static_cast<Token*>(le._token);

                        // remove it from the tracker list:
                        _list.erase(tmp);
                        ++count;
                    }
                }

                // reset the sentry.
                _list.splice(_list.begin(), _list, _sentryptr);
                _sentryptr = _list.begin();
            }
        };
    }
}
