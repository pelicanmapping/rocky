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
        static vsg::ref_ptr<vsg::ShaderSet> shaderSet;
        static Runtime* runtime;

        struct Config
        {
            //! Singleton pipeline config object created when the object is first constructed,
            //! for access to pipeline and desriptor set layouts.
            vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig;

            //! Singleton state commands for establishing the pipeline.
            vsg::StateGroup::StateCommands pipelineStateCommands;
        };

        enum Features
        {
            NONE = 0x0,
            WRITE_DEPTH = 0x1,
            TEXTURE = 0x2,
            DYNAMIC_STYLE = 0x4
        };

        static std::vector<Config> configs;

        //! Access a state config permutation
        static Config& get(int features);
    };

    /**
    * Command to bind any descriptors associated with Mesh.
    */
    class ROCKY_VSG_EXPORT BindMeshStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindMeshStyle>
    {
    public:
        //! Construct a default mesh styling command
        BindMeshStyle();

        //! Style for any linestrings that are children of this node
        void updateStyle(const MeshStyle&);

        vsg::ref_ptr<vsg::ubyteArray> _styleData;
        vsg::ref_ptr<vsg::ImageInfo> _imageInfo;

        void build(vsg::ref_ptr<vsg::PipelineLayout> layout);
    };

    /**
    * Command to render a Mesh's triangles.
    */
    class ROCKY_VSG_EXPORT MeshGeometry : public vsg::Inherit<vsg::Geometry, MeshGeometry>
    {
    public:
        //! Construct a new line string geometry node
        MeshGeometry();

        //! Adds a triangle to the mesh.
        //! Each array MUST be 3 elements long
        void add(
            const vsg::vec3* verts,
            const vsg::vec2* uvs,
            const vsg::vec4* colors,
            const float* depthoffsets);

        inline void add(
            const vsg::dvec3* verts,
            const vsg::vec2* uvs,
            const vsg::vec4* colors,
            const float* depthoffsets);

        //! Recompile the geometry after making changes.
        //! TODO: just make it dynamic instead
        void compile(vsg::Context&) override;

        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::vector<vsg::vec3> _verts;
        std::vector<vsg::vec3> _normals;
        std::vector<vsg::vec4> _colors;
        std::vector<vsg::vec2> _uvs;
        std::vector<float> _depthoffsets;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        using index_type = unsigned int; // short;
        using key = std::tuple<vsg::vec3, vsg::vec4>; // vert, color (add normal?)
        std::map<key, index_type> _lut;
        std::vector<index_type> _indices;
    };

    // inline
    void MeshGeometry::add(const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors, const float* depthoffsets)
    {
        vsg::vec3 verts32[3] = { vsg::vec3(verts[0]), vsg::vec3(verts[1]), vsg::vec3(verts[2]) };
        add(verts32, uvs, colors, depthoffsets);
    }
}
