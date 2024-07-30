/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once
#include <rocky/Bing.h>
#ifdef ROCKY_HAS_BING

#include <rocky/ImageLayer.h>
#include <rocky/LRUCache.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Image layer reading from Microsoft's Bing Maps API
     */
    class ROCKY_EXPORT BingImageLayer : public Inherit<ImageLayer, BingImageLayer>, public Bing::ImageLayerOptions
    {
    public:
        //! Construct an empty Bing layer
        BingImageLayer();
        BingImageLayer(const std::string& JSON, const IOOptions& io);

        //! Destructor
        virtual ~BingImageLayer() { }

        //! serialize
        JSON to_json() const override;

    protected: // Layer

        using TileURICache = util::LRUCache<TileKey, optional<Result<URI>>>;

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const override;

        std::unique_ptr<TileURICache> _tileURICache;

    private:

        void construct(const std::string& JSON, const IOOptions& io);
    };
}


#else // if !ROCKY_HAS_BING
#ifndef ROCKY_BUILDING_SDK
#error Bing support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_BING
