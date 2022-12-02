/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>

namespace rocky { namespace util
{
    class ROCKY_EXPORT Metrics
    {
    public:
        static void frame();

        static bool enabled();

        static void setEnabled(bool enabled);

        static void setGPUProfilingEnabled(bool enabled);
    };
} }

#ifdef ROCKY_PROFILING

// uncomment this to attempt GPU profiling blocks
//#define ROCKY_GPU_PROFILING

#define TRACY_ENABLE
#define TRACY_ON_DEMAND
#define TRACY_DELAYED_INIT
#include <Tracy.hpp>

#define ROCKY_PROFILING_ZONE ZoneNamed( ___tracy_scoped_zone, osgEarth::Util::Metrics::enabled() )
#define ROCKY_PROFILING_ZONE_NAMED(functionName) ZoneNamedN(___tracy_scoped_zone, functionName, osgEarth::Util::Metrics::enabled())
#define ROCKY_PROFILING_ZONE_COLOR(color) ZoneScopedC(___tracy_scoped_zone, color, osgEarth::Util::Metrics::enabled())
#define ROCKY_PROFILING_ZONE_TEXT(text) _zoneSetText(___tracy_scoped_zone, text)
#define ROCKY_PROFILING_PLOT(name, value) if (osgEarth::Util::Metrics::enabled()) {TracyPlot(name, value);}
#define ROCKY_PROFILING_FRAME_MARK if (osgEarth::Util::Metrics::enabled()) {FrameMark;}
#define ROCKY_LOCKABLE(type, varname) TracyLockable(type, varname)
#define ROCKY_LOCKABLE_NAMED(type, varname, desc) TracyLockableN(type, varname, desc)
#define ROCKY_LOCKABLE_BASE( type ) LockableBase( type )

#define ROCKY_PROFILING_GPU_ZONE(name)

inline void _zoneSetText(tracy::ScopedZone& zone, const char* text) {
    zone.Text(text, strlen(text));
}
inline void _zoneSetText(tracy::ScopedZone& zone, const std::string& text) {
    zone.Text(text.c_str(), text.size());
}

#else // no profiling - stub out

#define ROCKY_PROFILING_ZONE
#define ROCKY_PROFILING_ZONE_NAMED(functionName)
#define ROCKY_PROFILING_ZONE_COLOR(color)
#define ROCKY_PROFILING_ZONE_TEXT(text)
#define ROCKY_PROFILING_PLOT(name, value)
#define ROCKY_PROFILING_FRAME_MARK
#define ROCKY_LOCKABLE(type, varname) type varname
#define ROCKY_LOCKABLE_BASE( type ) type
#define ROCKY_PROFILING_GPU_ZONE(name)

#endif
