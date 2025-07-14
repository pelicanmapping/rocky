/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once
#include <rocky/Azure.h>
#ifdef ROCKY_HAS_AZURE

#include <rocky/ImageLayer.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Image layer reading from Microsoft's Azure Maps REST API
     * https://learn.microsoft.com/en-us/rest/api/maps/render/get-map-tile?view=rest-maps-2024-04-01
     */
    class ROCKY_EXPORT AzureImageLayer : public Inherit<ImageLayer, AzureImageLayer>,
        public Azure::Options
    {
    public:
        //! Construct an empty Azure layer
        AzureImageLayer();

        //! Deserialize an Azure Maps layer
        AzureImageLayer(std::string_view JSON, const IOOptions& io);

        //! serialize
        std::string to_json() const override;

    protected: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

    private:

        void construct(std::string_view JSON, const IOOptions& io);

        URIContext _uriContext;
    };
}


#else // if !ROCKY_HAS_AZURE
#ifndef ROCKY_BUILDING_SDK
#error Azure support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_AZURE
