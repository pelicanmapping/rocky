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

#include <functional>

namespace ROCKY_NAMESPACE
{
    class ElevationSession;

    /**
    * Style information for compiling and displaying Features
    */
    struct StyleSheet
    {
        LineStyle line;
        MeshStyle mesh;

        std::function<MeshStyle(const Feature&)> mesh_function;
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
        struct Primitives
        {
            Line line;
            Mesh mesh;

            inline bool empty() const {
                return line.points.empty() && mesh.triangles.empty();
            }

            //! Creates components for the primitive data and moves them
            //! into the registry. After calling this method, the member
            //! primitives are reset.
            inline entt::entity move(entt::registry& r) {
                if (empty())
                    return entt::null;

                auto e = r.create();

                if (!line.points.empty())
                {
                    r.emplace<Line>(e, std::move(line));
                }
                if (!mesh.triangles.empty())
                {
                    r.emplace<Mesh>(e, std::move(mesh));
                }
                return e;
            }
        };

        //! Return value from FeatureView generate().
        struct PrimitivesRef
        {
            Line* line = nullptr;
            Mesh* mesh = nullptr;
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

    public:
        //! Default construct - no data
        FeatureView() = default;

        //! Create geometry primitives from the feature list.
        //! Note: this method MAY modify the Features in the feature collection.
        //! @param srs SRS of resulting geometry; Usually this should be the World SRS of your map.
        //! @param runtime Runtime operations interface
        //! @return Collection of primtives representing the feature geometry
        Primitives generate(const SRS& output_srs);

        void generate(PrimitivesRef& working, const SRS& output_srs);
    };
}
