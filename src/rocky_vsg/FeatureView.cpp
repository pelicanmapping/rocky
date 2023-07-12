/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "FeatureView.h"
#include "engine/Runtime.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    template<class T>
    void tessellate_line(const T& from, const T& to, const SRS& srs, const GeodeticInterpolation interp, float max_span, std::vector<T>& output, bool add_last_point)
    {
        auto& ellipsoid = srs.ellipsoid();
        std::list<T> list{ from, to };
        auto iter = list.begin();
        for (;;)
        {
            auto save = iter;
            auto& p1 = *iter++;
            if (iter == list.end())
                break;
            auto& p2 = *iter;

            if (ellipsoid.geodesicGroundDistance(p1, p2) > max_span)
            {
                T midpoint;

                if (interp == GeodeticInterpolation::GreatCircle)
                {
                    ellipsoid.geodesicInterpolate(p1, p2, 0.5, midpoint);
                }
                else // GeodeticInterpolation::RhumbLine
                {
                    midpoint = (p1 + p2) * 0.5;
                }
                list.insert(iter, midpoint);
                iter = save;
            }
        }

        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            auto next = iter; next++;
            if (add_last_point || next != list.end())
                output.push_back(*iter);
        }
    }

    std::vector<glm::dvec3> tessellate_linestring(const std::vector<glm::dvec3>& input, const SRS& srs, GeodeticInterpolation interp, float max_span)
    {
        std::vector<glm::dvec3> output;
        if (input.size() > 0)
        {
            for (unsigned i = 1; i < input.size(); ++i)
            {
                tessellate_line(input[i - 1], input[i], srs, interp, max_span, output, false);
            }
            output.push_back(input.back());
        }
        return std::move(output);
    }

    float get_max_segment_length(const std::vector<glm::dvec3>& input)
    {
        float m = 0.0f;
        for (unsigned i = 0; i < input.size() - 1; ++i)
        {
            m = std::max(m, (float)glm::length(input[i] - input[i + 1]));
        }
        return m;
    }

    shared_ptr<Attachment> compile_feature_to_lines(const Feature& feature, const StyleSheet& styles)
    {
        float max_span = 100000.0;

        if (styles.line.has_value())
            max_span = styles.line->resolution;

        auto multiline = rocky::MultiLineString::create();

        float final_max_span = max_span;

        Geometry::const_iterator iter(feature.geometry);
        while (iter.hasMore())
        {
            auto& part = iter.next();

            // tessellate:
            auto tessellated = tessellate_linestring(part.points, feature.srs, feature.interpolation, max_span);

            // transform:
            auto feature_to_world = feature.srs.to(SRS::ECEF);
            feature_to_world.transformRange(tessellated.begin(), tessellated.end());

            // make the line attachment:
            multiline->pushGeometry(tessellated.begin(), tessellated.end());

            final_max_span = std::max(final_max_span, get_max_segment_length(tessellated));
        }

        // max length:
        max_span = final_max_span;

        if (styles.line.has_value())
        {
            multiline->setStyle(styles.line.value());
        }

        return multiline;
    }

    shared_ptr<Attachment> compile_feature_to_polygons(const Feature& feature, const StyleSheet& styles)
    {
        return compile_feature_to_lines(feature, styles);

        // TODO!
    }
}



FeatureView::FeatureView()
{
    //nop
}

FeatureView::FeatureView(const Feature& f)
{
    features.emplace_back(f);
}

FeatureView::FeatureView(Feature&& f)
{
    features.emplace_back(f);
}

void
FeatureView::createNode(Runtime& runtime)
{
    if (attachments.empty())
    {
        for (auto& feature : features)
        {
            if (feature.geometry.type == Geometry::Type::LineString ||
                feature.geometry.type == Geometry::Type::MultiLineString)
            {
                auto att = compile_feature_to_lines(feature, styles);
                if (att)
                {
                    attachments.emplace_back(att);
                }
            }
            else if (
                feature.geometry.type == Geometry::Type::Polygon ||
                feature.geometry.type == Geometry::Type::MultiPolygon)
            {
                auto att = compile_feature_to_polygons(feature, styles);
                if (att)
                {
                    attachments.emplace_back(att);
                }
            }
        }

        // invoke the super to bring it together.
        super::createNode(runtime);
    }
}
