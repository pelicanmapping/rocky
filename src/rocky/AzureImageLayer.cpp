/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "AzureImageLayer.h"
#ifdef ROCKY_HAS_AZURE

#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::Azure;

#undef LC
#define LC "[Azure] "

ROCKY_ADD_OBJECT_FACTORY(AzureImage,
    [](const JSON& conf) { return AzureImageLayer::create(conf); })

AzureImageLayer::AzureImageLayer() :
    super()
{
    construct(JSON());
}

AzureImageLayer::AzureImageLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
AzureImageLayer::construct(const JSON& conf)
{
    setConfigKey("AzureImage");
    const auto j = parse_json(conf);
    get_to(j, "subscription_key", subscriptionKey);
    get_to(j, "tileset_id", tilesetId);
    get_to(j, "map_tile_api_url", mapTileApiUrl);
}

JSON
AzureImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "subscription_key", subscriptionKey);
    set(j, "tileset_id", tilesetId);
    set(j, "map_tile_api_url", mapTileApiUrl);
    return j.dump();
}

Status
AzureImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    _profile = Profile::SPHERICAL_MERCATOR;
    setDataExtents({ _profile->extent() });

    // test fetch to make sure the API key is valid
    TileKey test(0, 0, 0, _profile);
    auto result = createImageImplementation(test, io);
    if (result.status.failed())
    {
        return Status(Status::ResourceUnavailable, result.status.message);
    }


    ROCKY_TODO("When disk cache is implemented, disable it here (or come up with a mechanism to ensure it only lasts six months/the period specified in the response header) to comply with ToS.");

    ROCKY_TODO("update attribution - it's a separate API call and depends on the visible region and zoom level, or can be queried for individual tiles, or there's an API to get a big JSON object with strings for each region of the world all at once");

    return StatusOK;
}

void
AzureImageLayer::closeImplementation()
{
    super::closeImplementation();
}

Result<GeoImage>
AzureImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
{
    ROCKY_PROFILE_FUNCTION();

    auto zoom = key.levelOfDetail();
    auto x = key.tileX();
    auto y = key.tileY();

    std::stringstream query;
    query << "?api-version=2024-04-01";
    query << "&tilesetId=" << tilesetId.value();
    query << "&zoom=" << zoom << "&x=" << x << "&y=" << y;
    // can be 256 or 512 - possibly worth making configurable
    query << "&tileSize=512";
    // can also authenticate with headers set in mapTileApiUrl
    if (subscriptionKey.has_value())
        query << "&subscription-key=" << subscriptionKey.value();

    URI imageURI(mapTileApiUrl->full() + query.str(), mapTileApiUrl->context());

    auto fetch = imageURI.read(io);
    if (fetch.status.failed())
    {
        return fetch.status;
    }

    std::istringstream buf(fetch->data);
    auto image_rr = io.services.readImageFromStream(buf, fetch->contentType, io);

    if (image_rr.status.failed())
        return image_rr.status;

    shared_ptr<Image> image = image_rr.value;

    if (image)
        return GeoImage(image, key.extent());
    else
        return StatusError;
}

#endif // ROCKY_HAS_AZURE
