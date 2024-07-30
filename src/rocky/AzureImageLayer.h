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
     * Image layer reading from Microsoft's Azure Maps API
     */
    class ROCKY_EXPORT AzureImageLayer : public Inherit<ImageLayer, AzureImageLayer>, public Azure::Options
    {
    public:
        //! Construct an empty Azure layer
        AzureImageLayer();
        AzureImageLayer(const std::string& JSON, const IOOptions& io);

        //! Destructor
        virtual ~AzureImageLayer() { }

        //! serialize
        JSON to_json() const override;

    protected: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

    private:

        void construct(const std::string& JSON, const IOOptions& io);
    };
}


#else // if !ROCKY_HAS_AZURE
#ifndef ROCKY_BUILDING_SDK
#error Azure support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_AZURE
