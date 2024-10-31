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
    [](const std::string& JSON, const IOOptions& io) {
        return AzureImageLayer::create(JSON, io); })

AzureImageLayer::AzureImageLayer() :
    super()
{
    construct({}, {});
}

AzureImageLayer::AzureImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
AzureImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("AzureImage");

    const auto j = parse_json(JSON);
    get_to(j, "subscription_key", subscriptionKey);
    get_to(j, "tileset_id", tilesetId);
    get_to(j, "map_tile_api_url", mapTileApiUrl);

    // environment variable key overrides a key set in code
    auto key = util::getEnvVar("AZURE_KEY");
    if (!key.empty())
    {
        Log()->info(LC "Loading subscription key from an environment variable");
        subscriptionKey.clear();
        subscriptionKey.set_default(key);
    }
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

    setProfile(Profile::SPHERICAL_MERCATOR);
    setDataExtents({ profile().extent() });

    // copy this so we can add headers
    _uriContext = mapTileApiUrl->context();
    _uriContext.headers.emplace("subscription-key", subscriptionKey.value());

    // test fetch to make sure the API key is valid
    TileKey test(1, 0, 0, profile());
    auto result = createImageImplementation(test, io);
    if (result.status.failed())
    {
        Log()->warn(LC "Failed to fetch test tile: {}", result.status.message);
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
    auto zoom = key.levelOfDetail();
    auto x = key.tileX();
    auto y = key.tileY();

    std::stringstream query;
    query << "?api-version=" << apiVersion.value();
    query << "&tilesetId=" << tilesetId.value();
    query << "&zoom=" << zoom << "&x=" << x << "&y=" << y;
    query << "&tileSize=" << tileSize().value();

    // note: _uriContext holds our authentication headers
    URI imageURI(mapTileApiUrl->full() + query.str(), _uriContext);

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
        return Status(Status::ResourceUnavailable);
}

#endif // ROCKY_HAS_AZURE
