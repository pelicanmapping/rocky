/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/TileLayer.h>
#include <rocky/Color.h>
#include <functional>

namespace ROCKY_NAMESPACE
{
    /** Ordered surface artwork in tile-local coordinates: TileUV or an isotropic Mercator frame.
     * Coordinates may extend outside the tile to preserve boundary strokes and coverage.
     * This is a renderer-independent intermediate representation, not an MVT file.
     */
    struct ROCKY_EXPORT VectorTile
    {
        struct Shape
        {
            enum class Type { Fill, Line };
            Type type = Type::Fill;
            //! Mercator uses isotropic units of tile Mercator width, with Y zero at the tile's latitude midpoint.
            //! TileUV maps (0,0) southwest to (1,1) northeast.
            enum class Coordinates { TileUV, Mercator };
            Coordinates coordinates = Coordinates::TileUV;
            //! Fill: exterior followed by holes. Line: independent open paths.
            std::vector<std::vector<glm::dvec2>> contours;
            //! Additional polygons in the same fill operation; each contains an exterior then holes.
            std::vector<std::vector<std::vector<glm::dvec2>>> polygons;
            Color color = StockColor::White;
            //! Line width in tile coordinate units; fixed in the resource's coordinate space.
            float width = 0.01f;
            //! Optional empty center of a line, in tile units. Width then specifies each side of its casing.
            float gap = 0.0f;
            //! Adjacent equal-colored shapes of the same type form one paint operation.
            //! Disjoint fill coverage is summed; overlapping round line coverage uses its maximum.
            //! Zero paints this shape independently. IDs are local to this VectorTile.
            std::uint32_t coverageGroup = 0u;
        };

        //! Painter's order; a feature may contribute several shapes (e.g., casing and road core).
        std::vector<Shape> shapes;

        //! Checks finite coordinates, valid contours, colors, widths, and bounded input size.
        Result<> validate() const;
    };

    /** Supplies vector surface artwork directly to terrain tiles.
     * The producer receives a destination terrain key and must reproject/retile its source into that key.
     * Level limits refer to destination terrain levels. No ECS entities or projectors are involved.
     */
    class ROCKY_EXPORT VectorTileLayer : public Inherit<TileLayer, VectorTileLayer>
    {
    public:
        //! Called concurrently on background workers. Set before open; do not mutate while open.
        //! Return an empty tile for valid empty coverage, or Failure for unavailable/canceled content.
        std::function<Result<VectorTile>(const TileKey&, const IOOptions&)> createTileFunction;

        //! Constructs a programmatic terrain vector layer. Native source drivers can subclass it.
        VectorTileLayer();
        //! Restores common tile/layer options; a programmatic producer must still be supplied by the caller.
        VectorTileLayer(std::string_view JSON, const IOOptions& io);

        //! Creates and validates artwork while preventing concurrent close. Does not cache failures.
        Result<VectorTile> createTile(const TileKey& key, const IOOptions& io,
            std::optional<double> styleZoom = {}) const;

        //! Captures this provider's view-dependent style input; nullopt means view-independent artwork.
        //! Called during update. The captured value travels with the request, never mutating layer state.
        virtual std::optional<double> viewStyleZoom(double zoom) const { return {}; }

        //! Selects an ancestor for overzoom, or INVALID outside this layer's destination level limits.
        TileKey bestAvailableTileKey(const TileKey& key) const override;

    protected:
        //! Rejects use when Slughorn is unavailable; source configuration is checked by the producer.
        Result<> openImplementation(const IOOptions& io) override;

        //! Runs the configured producer. Override for a source driver; caller holds the layer read lock.
        virtual Result<VectorTile> createTileImplementation(const TileKey& key, const IOOptions& io,
            std::optional<double> styleZoom) const;
    };
}
