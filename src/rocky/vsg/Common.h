/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <vsg/all.h>

#define ROCKY_VSG_NAMESPACE ROCKY_NAMESPACE

// non-exported classes
#define ROCKY_VSG_INTERNAL

#ifndef VSG_COMPUTE_VERSION
#define VSG_COMPUTE_VERSION(major, minor, rev) ((major) * 10000 + (minor) * 100 + (rev))
#define VSG_VERSION_INTEGER VSG_COMPUTE_VERSION(VSG_VERSION_MAJOR, VSG_VERSION_MINOR, VSG_VERSION_PATCH)
#endif

#if VSG_API_VERSION_LESS(1, 1, 7)
#error "Rocky requires VSG 1.1.7 or later"
#endif
