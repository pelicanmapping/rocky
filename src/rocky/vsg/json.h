/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
//#ifdef ROCKY_LIBRARY
//#define ROCKY_EXPOSE_JSON_FUNCTIONS

#include <rocky/Common.h>
#include <rocky/json.h>

namespace ROCKY_NAMESPACE
{
    ROCKY_DEFINE_JSON_SERIALIZERS(MapNode);
}
//#endif // ROCKY_LIBRARY
