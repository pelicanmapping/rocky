/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Feature.h>
#include <rocky/vsg/ecs.h>
#include <rocky/vsg/VSGContext.h>

#include <optional>
#include <functional>

namespace ROCKY_NAMESPACE
{
    /**
    * Style information for compiling and displaying Features
    */
    struct StyleSheet
    {
        std::optional<LineStyle> line;
        std::optional<MeshStyle> mesh;
        std::optional<IconStyle> icon;

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
    *  - Call generate to create a collection of entt::entity representing the geometry.
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

            inline entt::entity moveToEntity(entt::registry& r) {
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

    public:
        //! Collection of features to process
        std::vector<rocky::Feature> features;

        //! Styles to use when compiling features
        StyleSheet styles;

        //! Reference point (optional) to use for geometry localization.
        //! If you set this, make sure to add a corresponding Transform component
        //! to each of the resulting entities.
        GeoPoint origin;

    public:
        //! Default construct - no data
        FeatureView() = default;

        //! Create geometry primitives from the feature list.
        //! Note: this method MAY modify the Features in the feature collection.
        //! @param srs SRS of resulting geometry; Usually this should be the World SRS of your map.
        //! @param runtime Runtime operations interface
        //! @return Collection of primtives representing the feature geometry
        Primitives generate(const SRS& output_srs, VSGContext& runtime);
    };
}
