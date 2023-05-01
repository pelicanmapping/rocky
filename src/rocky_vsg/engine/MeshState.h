/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky/Status.h>
#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>

namespace ROCKY_NAMESPACE
{
    struct MeshStyle;
    class Runtime;

    /**
     * Creates commands for rendering mesh primitives and holds the singleton pipeline
     * configurator for their drawing state.
     */
    class ROCKY_VSG_EXPORT MeshState
    {
    public:
        //! Destructs the mesh state objects
        ~MeshState();

        //! Create the state commands necessary for rendering meshes.
        static void initialize(Runtime&);

        // Status; check before using.
        static Status status;

        //! Singleton pipeline config object created when the object is first constructed,
        //! for access to pipeline and desriptor set layouts.
        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig;

        //! Singleton state commands for establishing the pipeline.
        static vsg::StateGroup::StateCommands pipelineStateCommands;
    };

    /**
    * Applies a mesh style.
    */
    class ROCKY_VSG_EXPORT BindMeshStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindMeshStyle>
    {
    public:
        //! Construct a default mesh styling command
        BindMeshStyle();

        //! Style for any linestrings that are children of this node
        void setStyle(const MeshStyle&);
        const MeshStyle& style() const;

        vsg::ref_ptr<vsg::ubyteArray> _styleData;
    };

    /**
    * Renders a mesh geometry.
    */
    class ROCKY_VSG_EXPORT MeshGeometry : public vsg::Inherit<vsg::Geometry, MeshGeometry>
    {
    public:
        //! Construct a new line string geometry node
        MeshGeometry();

        //! Adds a triangle to the mesh.
        void add(const vsg::vec3& v1, const vsg::vec3& v2, const vsg::vec3& v3);

        //! Recompile the geometry after making changes.
        //! TODO: just make it dynamic instead
        void compile(vsg::Context&) override;
        
        void record(vsg::CommandBuffer&) const override;

    protected:
        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::vector<vsg::vec3> _verts;
        std::vector<vsg::vec3> _normals;
        std::vector<vsg::vec4> _colors;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        using index_type = unsigned short;
        std::map<vsg::vec3, index_type> _lut;
        std::vector<index_type> _indices;
    };
}
