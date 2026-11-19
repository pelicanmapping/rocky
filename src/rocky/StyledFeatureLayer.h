/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#pragma once
#include <rocky/VectorTileLayer.h>
#include <rocky/MapboxStyle.h>
#include <atomic>

namespace ROCKY_NAMESPACE
{
    //! Terrain surface provider for Mapbox/MapLibre styles and tiled or spatially queried feature sources.
    //! Configure before open. Reprojects into destination profiles using Rocky SRS operations.
    class ROCKY_EXPORT StyledFeatureLayer : public Inherit<VectorTileLayer, StyledFeatureLayer>
    {
    public:
        //! Style document URI; JSON "style" resolves relative to the referring map file.
        URI styleURI;
        //! Inline v8 style document, mutually exclusive with styleURI. Configure before open.
        std::string styleJSON;
        //! JSON object of source-ID bindings to GDAL or TiledFeatures configurations; configure before open.
        std::string sourceBindings;
        //! Memory budget for decoded features; raw URI content uses IOOptions::services().contentCache.
        std::size_t decodedCacheBytes = 128u * 1024u * 1024u;
        unsigned maximumSourceZoom = 14u;
        //! Diagnostic/reference path; set before open to buffer all lines on the CPU.
        bool forceBufferedStrokes = false;
        //! Configure before open. False uses the explicitly assigned style zoom in every view.
        bool automaticStyleZoom = true;

        //! Constructs an unopened styled vector layer.
        StyledFeatureLayer();
        //! Restores configuration without reading sources; URI context comes from the referring map file.
        StyledFeatureLayer(std::string_view JSON, const IOOptions& io);
        //! Serializes configuration only; runtime caches, counters and camera snapshots are not persisted.
        std::string to_json() const override;
        //! Sets zoom for direct createTile calls and automaticStyleZoom=false; quantizes to quarter levels.
        //! Invalidates artwork; call on the update thread. Terrain normally captures its own view zoom.
        //! Source zoom is separately capped by destination footprint and available source detail.
        void setStyleZoom(double zoom);
        //! Returns a quantized snapshot for a terrain view without invalidating other views.
        std::optional<double> viewStyleZoom(double zoom) const override;
        //! Current quantized style zoom; safe to inspect concurrently with tile production.
        double styleZoom() const
        {
            return _styleZoom.load();
        }
        //! Immutable style diagnostics and provider attribution; available after open.
        MapboxStyle style() const;
        //! Provider-supplied attribution text; applications choose its presentation.
        std::string attributionText() const;
        struct Stats
        {
            std::uint64_t reads = 0, hits = 0, tilesBuilt = 0, features = 0;
            std::uint64_t gpuStrokePieces = 0, bufferedStrokeBatches = 0;
            std::size_t cacheBytes = 0, cachedTiles = 0;
        };
        //! Thread-safe cumulative counters for demos and diagnostics.
        Stats stats() const;

    protected:
        //! Resolves style and source metadata while the base layer holds its write lock.
        Result<> openImplementation(const IOOptions&) override;
        //! Releases source/cache state after active tile producers have finished.
        void closeImplementation() override;
        //! Builds one destination tile on a worker while the base layer holds its read lock.
        Result<VectorTile> createTileImplementation(const TileKey&, const IOOptions&, std::optional<double>) const override;

    private:
        //! Parses provider options and retains configuration errors for open().
        void construct(std::string_view JSON, const IOOptions& io);
        Status _configurationStatus;
        std::string _referrer;
        struct Impl;
        std::shared_ptr<Impl> _impl;
        std::atomic<double> _styleZoom{12.0};
    };
} // namespace ROCKY_NAMESPACE
