/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "BingImageLayer.h"
#ifdef ROCKY_HAS_BING

#include "Context.h"
#include "json.h"
#include "Utils.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::Bing;

#undef LC
#define LC "[Bing] "

ROCKY_ADD_OBJECT_FACTORY(BingImage,
    [](const std::string& JSON, const IOOptions& io) { return BingImageLayer::create(JSON, io); })

BingImageLayer::BingImageLayer() :
    super()
{
    construct({}, {});
}

BingImageLayer::BingImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
BingImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("BingImage");

    const auto j = parse_json(JSON);
    get_to(j, "key", apiKey);
    get_to(j, "imagery_set", imagerySet);
    get_to(j, "imagery_metadata_api_url", imageryMetadataUrl, io);

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
BingImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "key", apiKey);
    set(j, "imagery_set", imagerySet);
    set(j, "imagery_metadata_api_url", imageryMetadataUrl);
    return j.dump();
}

Status
BingImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    // Bing has a root 2x2 tile setup unlike most web-mercator sources:
    profile = Profile(SRS::SPHERICAL_MERCATOR, Profile::SPHERICAL_MERCATOR.extent().bounds(), 2, 2);
    
    setDataExtents({ profile.extent() });

    ROCKY_TODO("When disk cache is implemented, disable it here as it violates the ToS");

    return StatusOK;
}

void
BingImageLayer::closeImplementation()
{
    super::closeImplementation();
}

Result<GeoImage>
BingImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
{
    // Bing's zoom is indexed slightly differently
    auto zoom = key.level + 1;
    GeoPoint centre = key.extent().centroid();
    centre.transformInPlace(centre.srs.geodeticSRS());

    std::stringstream relative;
    relative << std::setprecision(12);
    relative << "/" << imagerySet.value();
    relative << "/" << centre.y << "," << centre.x;
    relative << "?zl=" << zoom;
    relative << "&o=json";
    relative << "&key=" << apiKey.value();

    URI metadataURI(imageryMetadataUrl->full() + relative.str(), imageryMetadataUrl->context());

    URI imageURI;

    // Bing is a 2-step process. First fetch the metadata for this tile:
    auto metaFetch = metadataURI.read(io);
    if (metaFetch.status.ok())
    {
        auto json = parse_json(metaFetch->data);
        const auto& vintage = json["/resourceSets/0/resources/0/vintageEnd"_json_pointer];
        const auto& jsonURI = json["/resourceSets/0/resources/0/imageUrl"_json_pointer];
        if (!vintage.empty() && !jsonURI.empty())
            imageURI = URI(jsonURI.get<std::string>(), imageryMetadataUrl->context());
        else
            return Status(Status::ResourceUnavailable, "No data");
    }
    else
    {
        if (metaFetch.status.message == "Unauthorized")
        {
            setStatus(metaFetch.status);
        }
        return metaFetch.status;
    }

    // Now fetch the actual image:
    auto fetch = imageURI.read(io);
    if (fetch.status.failed())
    {
        return fetch.status;
    }

    // Decode the stream:
    std::istringstream buf(fetch->data);
    auto image_rr = io.services.readImageFromStream(buf, fetch->contentType, io);

    if (image_rr.status.failed())
    {
        return image_rr.status;
    }

    auto image = image_rr.value;

    if (image)
        return GeoImage(image, key.extent());
    else
        return Status_ResourceUnavailable;
}

#endif // ROCKY_HAS_BING
