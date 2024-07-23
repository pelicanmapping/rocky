/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once
#include <rocky/Bing.h>

#ifdef ROCKY_HAS_BING

#include <rocky/ElevationLayer.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Elevation layer reading from from Microsoft's Bing Maps API
     */
    class ROCKY_EXPORT BingElevationLayer : public Inherit<ElevationLayer, BingElevationLayer>,
        public Bing::ElevationLayerOptions
    {
    public:
        //! Construct an empty Bing layer
        BingElevationLayer();
        BingElevationLayer(const JSON&);

        //! Destructor
        virtual ~BingElevationLayer() { }

        //! Serialize
        JSON to_json() const override;

        optional<Encoding> encoding;

    public: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const override;


    private:
        void construct(const JSON&);
    };
}

#else // if !ROCKY_HAS_BING
#ifndef ROCKY_BUILDING_SDK
#error Bing support is not enabled in Rocky.
#endif
#endif // ROCKY_HAS_BING
