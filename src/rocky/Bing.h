/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once

#include <rocky/Version.h>
#ifdef ROCKY_HAS_BING

#include <rocky/Common.h>
#include <rocky/optional.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    namespace Bing
    {
        struct ImageLayerOptions
        {
            optional<std::string> apiKey;
            optional<std::string> imagerySet = { "Aerial" };
            optional<URI> imageryMetadataUrl = { "https://dev.virtualearth.net/REST/v1/Imagery/Metadata" };
        };

        struct ElevationLayerOptions
        {
            optional<std::string> apiKey;
            optional<URI> url = { "http://dev.virtualearth.net/REST/v1/Elevation/Bounds" };
        };
    }
}

#else // if !ROCKY_HAS_BING
  #ifndef ROCKY_BUILDING_SDK
    #error Bing support is not enabled in Rocky.
  #endif
#endif // ROCKY_HAS_BING
