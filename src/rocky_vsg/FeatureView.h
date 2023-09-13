/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Feature.h>
#include <optional>
#include <functional>
#include <rocky_vsg/Line.h>
#include <rocky_vsg/Mesh.h>
#include <rocky_vsg/Icon.h>

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
    * FeatureView is an attachment that compiles a collection of Feature objects
    * for visualization.
    */
    class ROCKY_VSG_EXPORT FeatureView : public ECS::Component
    {
    public:
        //! Collection of features to view
        std::vector<rocky::Feature> features;

        //! Styles to use when compiling features
        StyleSheet styles;

        //! Create VSG geometry from the feature list
        //! @param registry Entity registry
        //! @param runtime Runtime operations interface
        //! @param keep_features Whether to keep the "features" vector intact;
        //!   by default it is cleared after calling generate
        void generate(
            ECS::Registry& registry,
            Runtime& runtime,
            bool keep_features = false);

        //! Whether to render this component
        bool active = true;

    public:
        //! Default construct - no data
        FeatureView();

        //! Construct a view to display a single feature
        FeatureView(const Feature& value);

        //! Construct a view to display a single moved feature)
        FeatureView(Feature&& value);
    };
}
