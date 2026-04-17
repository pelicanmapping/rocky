/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Feature.h>
#include <rocky/ElevationSampler.h>
#include <rocky/ecs/Line.h>
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    class ElevationSession;

    /**
    * Style information for compiling and displaying Features
    */
    struct StyleSheet
    {
        LineStyle lineStyle;
        MeshStyle meshStyle;

        // EXPERIMENTAL, may change
        std::function<Color(const Feature&)> meshColorFunction;
    };

    /**
    * FeatureView is a utility that compiles a collection of Feature objects
    * into renderable components.
    *
    * Usage:
    *  - Create a FeatureView
    *  - Populate the features vector
    *  - Optionally set styles for rendering
    *  - Call generate to create Line and Mesh primitives representing the geometry.
    */
    class ROCKY_EXPORT FeatureView
    {
    public:
        //! Return value from FeatureView generate().
        struct Workspace
        {
            LineGeometry lineGeom;
            MeshGeometry meshGeom;

            inline bool empty() const {
                return lineGeom.points.empty() && meshGeom.vertices.empty();
            }
        };

    public:
        //! Collection of features to process
        std::vector<rocky::Feature> features;

        //! Styles to use when compiling features
        StyleSheet styles;

        //! Reference point (optional) to use for geometry localization.
        //! If you set this, make sure to add a corresponding Transform component
        //! to each of the resulting entities.
        GeoPoint origin;

        //! An optional elevation sampler will create clamped geometry.
        ElevationSession clamper;

        //! A target entity to which to attach generated primitives.
        //! If you leave this at entt::null, a new entity will be created.
        entt::entity entity = entt::null;

    public:
        //! Default construct - no data
        FeatureView() = default;

        //! Create geometry primitives from the feature list.
        //! Note: this method MAY modify the Features in the feature collection.
        //! @param srs SRS of resulting geometry; Usually this should be the World SRS of your map.
        //! @param runtime Runtime operations interface
        //! @return Collection of primtives representing the feature geometry
        entt::entity generate(const SRS& output_srs, Registry& r);
        
        //! Utility: generate line geometry for a single feature with a given style, and append it to the provided LineGeometry.
        static void generateLine(const Feature& feature, const LineStyle& style, const GeoPoint& origin,
            ElevationSession& clamper, const SRS& outputSRS, LineGeometry& lineGeom);

        //! Utility generate mesh geometry from a single feature with a given style,
        //! appending it to the provided MeshGeometry.
        static void generateMesh(const Feature& feature, const MeshStyle& style,
            const GeoPoint& origin, ElevationSession& clamper, const SRS& output_srs, MeshGeometry& meshGeom);
    };
}
