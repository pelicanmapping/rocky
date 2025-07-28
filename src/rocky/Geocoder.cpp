/**
 * rocky c++
 * Copyright Pelican Mapping
 * MIT License
 */
#include "Geocoder.h"
#include "Feature.h"

#ifdef ROCKY_HAS_GEOCODER
#include "GDALFeatureSource.h"
#include <ogr_geocoding.h>
#endif

using namespace ROCKY_NAMESPACE;


Result<std::vector<Feature>>
Geocoder::geocode(const std::string& location, IOOptions& io) const
{
#ifdef ROCKY_HAS_GEOCODER

    std::vector<Feature> result;
    char* options[2] = { "WRITE_CACHE=FALSE", nullptr };
    auto session = OGRGeocodeCreateSession(options);
    if (session)
    {
        auto layerHandle = OGRGeocode(session, location.c_str(), nullptr, nullptr);
        if (layerHandle)
        {
            auto fs = GDALFeatureSource::create();
            fs->externalLayerHandle = layerHandle;
            fs->externalSRS = SRS::WGS84;
            auto iter = fs->iterate(io);
            while (iter.hasMore())
            {
                result.emplace_back(iter.next());
            }
            OGRGeocodeFreeResult(layerHandle);
        }
        OGRGeocodeDestroySession(session);
    }

    if (result.empty())
        return Failure(Failure::ResourceUnavailable, "No results found");
    else
        return result;

#else
    return Failure(Failure::ServiceUnavailable, "Geocoder service is not available");
#endif
}
