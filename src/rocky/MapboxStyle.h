/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#pragma once
#include <rocky/Feature.h>
#include <rocky/Color.h>
#include <memory>

namespace ROCKY_NAMESPACE
{
    //! Validated, immutable subset of the Mapbox/MapLibre v8 style specification.
    //! No rendering or network dependencies. Concurrent evaluation is safe after load().
    class ROCKY_EXPORT MapboxStyle
    {
    public:
        struct Source
        {
            std::string id, url;
            //! Single collections use GeoJSON source semantics; vector sources require source-layer.
            std::string type = "vector";
            std::vector<std::string> tiles;
            unsigned minZoom = 0, maxZoom = 14;
            std::string attribution;
        };
        enum class Type
        {
            Background,
            Fill,
            Line
        };
        struct Layer
        {
            std::string id, source, sourceLayer;
            Type type = Type::Fill;
            double minZoom = 0, maxZoom = 24;
        };
        struct Paint
        {
            Color color;
            double widthPixels = 1.0, gapPixels = 0.0;
        };

        //! Parses and validates a style transactionally. Unsupported layers/properties are diagnosed.
        //! A malformed root fails; unsupported expressions or paints omit their entire layer.
        Result<> load(std::string_view json);
        //! Metadata for the accepted vector sources and paint layers, in original painter order.
        const std::vector<Source>& sources() const;
        const std::vector<Layer>& layers() const;
        const std::vector<std::string>& diagnostics() const;
        //! Returns no paint for a filtered/hidden/out-of-zoom feature; errors are never silent defaults.
        //! Colors are linear RGB; widths are style pixels at the supplied continuous zoom.
        Result<std::optional<Paint>> evaluate(std::size_t layer, const Feature& feature, double zoom) const;

    private:
        struct Impl;
        std::shared_ptr<const Impl> _impl;
    };
} // namespace ROCKY_NAMESPACE
