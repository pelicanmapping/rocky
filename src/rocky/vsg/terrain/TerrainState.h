/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/Color.h>
#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    struct TerrainTileModel;
    class TerrainSettings;

    //! Holds any terrain-wide textures and uniforms.
    struct TerrainDescriptors
    {
        struct Uniforms
        {
            glm::vec4 backgroundColor = Color("#08AEE0");
            bool wireOverlay = false;
        };

        vsg::ref_ptr<vsg::Data> data;
        vsg::ref_ptr<vsg::Descriptor> ubo;
    };

    //! Descriptors for a single terrain tile.
    struct TerrainTileDescriptors
    {
        struct Uniforms
        {
            glm::fmat4 elevation_matrix;
            glm::fmat4 color_matrix;
            glm::fmat4 model_matrix;
            float min_height = 1.0f;
            float max_height = 0.0f;
            float padding[2];
        };
        vsg::ref_ptr<vsg::DescriptorImage> color;
        vsg::ref_ptr<vsg::DescriptorImage> elevation;
        vsg::ref_ptr<vsg::DescriptorBuffer> uniforms;
        vsg::ref_ptr<vsg::StateCommand> bind;
    };

    //! One texture source image and its matrix.
    struct TextureData
    {
        std::string name;
        std::shared_ptr<Image> image;
        glm::dmat4 matrix{ 1 };
    };

    //! Everything needed to render a single tile
    struct TerrainTileRenderModel
    {
        glm::fmat4 modelMatrix;
        TextureData color;
        TextureData elevation;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;

        TerrainTileDescriptors descriptors;

        void applyScaleBias(const glm::dmat4& sb)
        {
            if (color.image)
                color.matrix *= sb;
            if (elevation.image)
                elevation.matrix *= sb;
        }
    };

    /**
     * TerrainState creates all the Vulkan state necessary to
     * render the terrain.
     *
     * TODO: Eventually, this will need to integrate "upwards" to the 
     * MapNode and finally to the application level itself so we
     * do shader composition with some kind of uber-shader-with-defines
     * architecture.
     */
    class ROCKY_VSG_INTERNAL TerrainState
    {
    public:
        //! Initialize the factory
        TerrainState(VSGContext);

        //! Configures an existing stategroup for rendering terrain
        bool setupTerrainStateGroup(vsg::StateGroup& stateGroup, VSGContext& context);

        //! Integrates data from the new data model into an existing render model,
        //! and creates or updates all the necessary descriptors and commands.
        //! After calling this, you will need to install the find bind command
        //! in your stategroup.
        TerrainTileRenderModel updateRenderModel(
            const TerrainTileRenderModel& oldRenderModel,
            const TerrainTileModel& newDataModel,
            VSGContext& runtime) const;

        void updateSettings(const TerrainSettings&);

        //! Status of the factory.
        Status status;

    public:

        //! Config object for creating the terrain's graphics pipeline
        vsg::ref_ptr<vsg::GraphicsPipelineConfig> pipelineConfig;

        //! VSG parent shader set that we use to develop the terrain tile
        //! state group for each tile.
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        //! Default state descriptors for a terrain tile.
        //! This holds the "default" (i.e. empty) textures and uniforms
        //! that will populate a descriptor set when no other textures are available.
        //! Terrain tiles copy and use this until new data becomes available.
        TerrainTileDescriptors defaultTileDescriptors;

    protected:

        //! Creates all the default texture information,
        //! i.e. placeholder textures and uniforms for all tiles
        //! when they don't have actual data.
        void createDefaultDescriptors(VSGContext&);

        //! Creates the base shader set used when rendering terrain
        vsg::ref_ptr<vsg::ShaderSet> createShaderSet(VSGContext&) const;

        //! Creates a configurator for graphics pipeline state groups.
        //! The configurator does not contain any ACTUAL decriptors (like
        //! textures and uniforms) but rather just prepares the ShaderSet
        //! to work with the specific decriptors you PLAN to provide.
        vsg::ref_ptr<vsg::GraphicsPipelineConfig> createPipelineConfig(VSGContext&) const;

        //! Defines a single texutre and its (possible shared) sampler
        struct TextureDef
        {
            // name in the shader
            std::string name;

            // binding point (layout binding=X) in the shader
            uint32_t uniform_binding;

            // sampler to use
            vsg::ref_ptr<vsg::Sampler> sampler;

            // default placeholder texture data
            vsg::ref_ptr<vsg::Data> defaultData;
        };

        //! stock samplers to use for terrain textures
        struct
        {
            TextureDef color;
            TextureDef elevation;
        }
        texturedefs;

        // terrain-wide settings, etc.
        TerrainDescriptors _terrainDescriptors;
    };
}
