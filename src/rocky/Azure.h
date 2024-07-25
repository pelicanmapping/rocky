/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once

#include <rocky/Version.h>
#ifdef ROCKY_HAS_AZURE

#include <rocky/Common.h>
#include <rocky/optional.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    namespace Azure
    {
        struct Options
        {
            optional<std::string> subscriptionKey;
            optional<std::string> tilesetId = { "microsoft.base.darkgrey" };
            optional<URI> mapTileApiUrl = { "https://atlas.microsoft.com/map/tile" };
        };
    }
}

#else // if !ROCKY_HAS_AZURE
  #ifndef ROCKY_BUILDING_SDK
    #error Azure support is not enabled in Rocky.
  #endif
#endif // ROCKY_HAS_AZURE
