/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky_vsg/engine/LineState.h>

namespace ROCKY_NAMESPACE
{
    //! Settings when constructing a similar set of line drawables
    //! Note, this structure is mirrored on the GPU so alignment rules apply!
    struct LineStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        vsg::vec4 color = { 1, 1, 1, 0 };
        float width = 2.0f;
        int stipple_pattern = 0xffff;
        int stipple_factor = 1;
    };

    /**
    * LineString Attachment
    */
    class ROCKY_VSG_EXPORT LineString : public rocky::Inherit<Attachment, LineString>
    {
    public:
        //! Construct a line string attachment
        LineString();

        //! Add a vertex to the end of the line string
        void pushVertex(float x, float y, float z);

        //! Add a vertex to the end of the line string
        template<typename T> void pushVertex(const T& vec3);

        //! Set the rendering style for this line string
        void setStyle(const LineStyle&);

        //! rendering style for the geometry
        const LineStyle& style() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<BindLineStyle> _bindStyle;
        vsg::ref_ptr<LineStringGeometry> _geometry;
    };

    template<typename T> void LineString::pushVertex(const T& vec3) {
        pushVertex(vec3.x, vec3.y, vec3.z);
    }
}
