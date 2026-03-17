/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

// A place to initialize statics that require proper ordering
#include "Common.h"
#include "weejobs.h"

// job system definition
#ifdef WEEJOBS_USE_SINGLETON
WEEJOBS_INSTANCE;
#endif
