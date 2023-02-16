/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <mutex>
#include <vector>

namespace ROCKY_NAMESPACE
{
    namespace util
    {
        /**
        * Structure thats lets you store data on a "per view" basis.
        * You can index into this structure during a RecordTraversal
        * like so:
        *
        * ViewLocal<Data> viewlocal;
        * ...
        * auto view_data = viewlocal[t->getState()->_commandBuffer->viewID];
        */
        template<typename T> struct ViewLocal
        {
        public:
            //! Fetch the data associated with the view id
            T& operator[](std::uint32_t viewID) const
            {
                if (viewID >= _vdd.size())
                {
                    std::scoped_lock lock(_mutex);
                    if (viewID >= _vdd.size())
                    {
                        _vdd.resize(viewID + 1);
                    }
                }
                return _vdd[viewID];
            }
            using iterator = typename std::vector<T>::iterator;
            iterator begin() { return _vdd.begin(); }
            iterator end() { return _vdd.end(); }

        private:
            mutable std::mutex _mutex;
            mutable std::vector<T> _vdd;
        };
    }
}
