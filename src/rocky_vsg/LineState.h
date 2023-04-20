/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/RuntimeContext.h>

#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>

namespace ROCKY_NAMESPACE
{
    //! Settings when constructing a similar set of line drawables
    //! TODO: this may move to LineDrawable or whatever it's called
    struct LineStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        vsg::vec4 color = { 1, 1, 1, 0 };
        float width = 2.0f;
        unsigned short stipple_pattern = 0xffff;
        unsigned int stipple_factor = 1;
    };

    //! Internal - descriptors pertaining to a line drawable
    struct LineStyleDescriptors
    {
        struct Uniforms
        {
            vsg::vec4 color;
            int first = -1;
            int last = -1;
            float width;
            int stipple_pattern;
            int stipple_factor;
        };
    };

    /**
     * Creates a pipeline for rendering line primitives.
     */
    class ROCKY_VSG_EXPORT LineStateFactory
    {
    public:
        //! Construct the Line state generator and initialize its pipeline configurator
        LineStateFactory(RuntimeContext& runtime);

        //! Create the state commands necessary for rendering lines.
        //! Add these to an existing StateGroup.
        vsg::StateGroup::StateCommands createPipelineStateCommands() const;

        //! Create a descriptor set for rendering a particular line style.
        //! Add this to an existing StateGroup.
        vsg::ref_ptr<vsg::StateCommand> createBindDescriptorSetCommand(
            const LineStyle& style) const;

        //! Status of this object - will reflect an error if the factory
        //! could not initialize properly
        Status status;

    private:

        RuntimeContext& _runtime;
        vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> _config;
    };

    /**
    * Applies a line style to any LineStringNode children.
    */
    class ROCKY_VSG_EXPORT LineStringStyleNode : public vsg::Inherit<vsg::StateGroup, LineStringStyleNode>
    {
    public:
        //! Construct a line style node
        LineStringStyleNode(RuntimeContext& runtime);

        //! Set the style for any linestrings that are children of this node
        void setStyle(const LineStyle&);
        const LineStyle& style() const { return _style; }

    public: // VSG
        void compile(vsg::Context&) override;

    private:
        LineStyle _style;
        RuntimeContext& _runtime;
    };

    /**
    * Renders a line or linestring geometry.
    */
    class ROCKY_VSG_EXPORT LineStringNode : public vsg::Inherit<vsg::Geometry, LineStringNode>
    {
    public:
        LineStringNode();

        //! Adds a vertex to the end of the linestring.
        void push_back(const vsg::vec3& vert);

        unsigned numVerts() const;

        //! Sets the color of the linestring
        //void setColor(const vsg::vec4& color);

        //! Recompile the geometry after making changes.
        void compile(vsg::Context&) override;

    protected:
        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::uint32_t _stippleFactor;
        std::uint16_t _stipplePattern;
        float _width;
        std::vector<vsg::vec3> _current;
        std::vector<vsg::vec3> _previous;
        std::vector<vsg::vec3> _next;
        std::vector<vsg::vec4> _colors;

        //unsigned actualVertsPerVirtualVert(unsigned) const;
        //unsigned numVirtualVerts(const vsg::Array*) const;
        //unsigned getRealIndex(unsigned) const;
        //void updateFirstCount();
    };
}
