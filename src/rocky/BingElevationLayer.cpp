/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "BingElevationLayer.h"
#ifdef ROCKY_HAS_BING

#include "Context.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::Bing;

#undef LC
#define LC "[Bing] "

ROCKY_ADD_OBJECT_FACTORY(BingElevation, 
    [](std::string_view JSON, const IOOptions& io) {
        return BingElevationLayer::create(JSON, io); })

BingElevationLayer::BingElevationLayer() :
    super()
{
    construct({}, {});
}

BingElevationLayer::BingElevationLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
BingElevationLayer::construct(std::string_view JSON, const IOOptions& io)
{
    setLayerTypeName("BingElevation");

    const auto j = parse_json(JSON);
    get_to(j, "key", apiKey);
    get_to(j, "url", url, io);

    // environment variable key overrides a key set in code
    auto key = util::getEnvVar("BING_KEY");
    if (!key.empty())
    {
        Log()->info(LC "Overriding API key from environment variable");
        apiKey.clear();
        apiKey.set_default(key);
    }
}

JSON
BingElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "key", apiKey);
    set(j, "url", url);
    return j.dump();
}

Result<>
BingElevationLayer::openImplementation(const IOOptions& io)
{
    auto parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile SPHERICAL_MERCATOR("spherical-mercator");
    profile = Profile(SPHERICAL_MERCATOR.srs(), SPHERICAL_MERCATOR.extent().bounds(), 2, 2);
    setDataExtents({ profile.extent() });

    ROCKY_TODO("When disk cache is implemented, disable it here as it violates the ToS");

    ROCKY_TODO("Update attribution - it's included in the JSON response, but we don't track which tiles are still visible and only have the data in a const function");

    return {};
}

void
BingElevationLayer::closeImplementation()
{
    super::closeImplementation();
}

Result<GeoHeightfield>
BingElevationLayer::createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const
{
    if (status().failed())
        return status().error();

    GeoExtent latLongExtent = Profile("global-geodetic").clampAndTransformExtent(key.extent());

    unsigned tileSize = 32;
    std::stringstream query;
    query << std::setprecision(12);
    query << "?bounds=" << latLongExtent.ymin() << "," << latLongExtent.xmin() << "," << latLongExtent.ymax() << "," << latLongExtent.xmax();
    query << "&rows=" << tileSize;
    query << "&cols=" << tileSize;
    query << "&heights=ellipsoid";
    query << "&key=" << apiKey.value();

    URI dataURI(url->full() + query.str(), url->context());

    auto fetch = dataURI.read(io);
    if (fetch.failed())
        return fetch.error();

    auto json = parse_json(fetch->content.data);

    const auto& elevations = json["/resourceSets/0/resources/0/elevations"_json_pointer];
    if (elevations.empty())
        return Failure("JSON response contained no elevations");
    if (!elevations.is_array())
        return Failure("JSON response contained unexpected type");
    if (elevations.size() != tileSize * tileSize)
        return Failure("JSON response contained unexpected number of points");

    auto heightfield = Heightfield::create(tileSize, tileSize);
    auto jsonItr = elevations.begin();
    heightfield->forEachHeight([&](float& point) {point = (jsonItr++)->get<float>(); });

    if (heightfield)
        return GeoHeightfield(heightfield, key.extent());
    else
        return Failure_ResourceUnavailable;
}

#endif // ROCKY_HAS_BING
