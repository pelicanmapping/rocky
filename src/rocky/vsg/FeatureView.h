/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Feature.h>
#include <rocky/vsg/Line.h>
#include <rocky/vsg/Mesh.h>
#include <rocky/vsg/Icon.h>

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
    */
    class ROCKY_EXPORT FeatureView
    {
    public:
        //! Collection of features to view
        std::vector<rocky::Feature> features;

        //! Styles to use when compiling features
        StyleSheet styles;

        //! Create VSG geometry from the feature list
        //! @param registry Entity registry, locked for writing
        //! @param srs SRS or resulting geometry
        //! @param runtime Runtime operations interface
        //! @param keep_features Whether to keep the "features" vector intact;
        //!   by default it is cleared after calling generate
        void generate(
            entt::registry& registry,
            const SRS& srs,
            Runtime& runtime,
            bool keep_features = false);

        //! Deletes any geometries previously created by generate()
        //! @param registry Entity registry, locked for writing
        void clear(entt::registry& registry);

        //! Call if you change the stylesheet after generating.
        //! @param registry Entity registry, locked for reading
        void dirtyStyles(entt::registry& registry);

    public:
        //! Default construct - no data
        FeatureView();

        //! Construct a view to display a single feature
        FeatureView(const Feature& value);

        //! Construct a view to display a single moved feature)
        FeatureView(Feature&& value) noexcept;

        //! Host entities
        entt::entity entity = entt::null;
        std::vector<entt::entity> mesh_entities;
        std::vector<entt::entity> line_entities;
    };
}
