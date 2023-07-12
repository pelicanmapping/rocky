/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky/Feature.h>
#include <optional>
#include <rocky_vsg/LineString.h>
#include <rocky_vsg/Mesh.h>
#include <rocky_vsg/Icon.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Conveys geometry compile hints to the FeatureView attachment
    */
    struct StyleSheet
    {
        std::optional<LineStyle> line;
        std::optional<MeshStyle> mesh;
        std::optional<IconStyle> icon;
    };

    /**
    * FeatureView is an attachment that compiles a collection of Feature objects
    * for visualization.
    */
    class ROCKY_VSG_EXPORT FeatureView : public rocky::Inherit<AttachmentGroup, FeatureView>
    {
    public:
        //! Collection of features to view
        std::vector<rocky::Feature> features;

        //! Styles to use when compiling features
        StyleSheet styles;

    public:
        FeatureView();
        FeatureView(const Feature& value);
        FeatureView(Feature&& value);

    protected:
        void createNode(Runtime& runtime) override;

    private:
    };
}
