/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <cstdint>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT Memory
    {
    public:
        //! Physical memory usage, in bytes, for the calling process. (aka working set or resident set)
        static std::int64_t getProcessPhysicalUsage();

        //! Peak physical memory usage, in bytes, for the calling process since it started.
        static std::int64_t getProcessPeakPhysicalUsage();

        //! Private bytes allocated solely to this process
        static std::int64_t getProcessPrivateUsage();

        //! Maximum bytes allocated privately to thie process (peak pagefile usage)
        static std::int64_t getProcessPeakPrivateUsage();

        // Not creatable.
        Memory() = delete;
    };
}
