/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <chrono>

namespace rocky
{
    using clock = std::chrono::steady_clock;
}

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined( __BCPLUSPLUS__)  || defined( __MWERKS__)
    #  if defined( ROCKY_VSG_LIBRARY_STATIC )
    #    define ROCKY_VSG_EXPORT
    #  elif defined( ROCKY_VSG_LIBRARY )
    #    define ROCKY_VSG_EXPORT   __declspec(dllexport)
    #  else
    #    define ROCKY_VSG_EXPORT   __declspec(dllimport)
    #  endif
#else
    #  define ROCKY_VSG_EXPORT
#endif  

// marker for non-exported classes
#define ROCKY_VSG_INTERNAL