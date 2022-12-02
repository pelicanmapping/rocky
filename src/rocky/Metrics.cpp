/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include "Metrics.h"
#include <cstdlib>

using namespace rocky;
using namespace rocky::util;

#define LC "[Metrics] "

static bool s_metricsEnabled = true;
static bool s_gpuMetricsEnabled = true;
static bool s_gpuMetricsInstalled = false;

bool Metrics::enabled()
{
    return s_metricsEnabled;
}

void Metrics::setEnabled(bool enabled)
{
    s_metricsEnabled = enabled;
}

void Metrics::setGPUProfilingEnabled(bool enabled)
{
    s_gpuMetricsEnabled = enabled;
}

void Metrics::frame()
{
    ROCKY_PROFILING_FRAME_MARK;
}
