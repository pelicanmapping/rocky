/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/TerrainTileNode.h>

#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/SharedObjects.h>
#include <vsg/nodes/StateGroup.h>

namespace rocky
{
    class RuntimeContext;
    class TerrainTileNode;
    class TerrainTileRenderModel;

    /**
     * StateFactory creates all the Vulkan state necessary to
     * render the terrain.
     *
     * TODO: Eventually, this will need to integrate "upwards" to the 
     * MapNode and finally to the application level itself so we
     * do shader composition with some kind of uber-shader-with-defines
     * architecture.
     */
    class ROCKY_VSG_INTERNAL StateFactory
    {
    public:
        //! Initialize the factory
        StateFactory();

        //! Creates a state group for rendering terrain
        virtual vsg::ref_ptr<vsg::StateGroup> createTerrainStateGroup() const;

        //! Creates a state group for rendering a specific terrain tile
        virtual void updateTerrainTileDescriptors(
            const TerrainTileRenderModel& renderModel,
            vsg::ref_ptr<vsg::StateGroup> stategroup,
            RuntimeContext& runtime) const;

    public:

        //! Config object for creating the terrain's graphics pipeline
        vsg::ref_ptr<vsg::GraphicsPipelineConfig> pipelineConfig;

        //! VSG parent shader set that we use to develop the terrain tile
        //! state group for each tile.
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        //! Cache of VSG objects shared by different parts of the
        //! terrain rendering subsystem.
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! Default state descriptors for a terrain tile.
        //! This holds the "default" (i.e. empty) textures and uniforms
        //! that will populate a descriptor set when no other textures are available.
        //! Terrain tiles copy and use this until new data becomes available.
        TerrainTileDescriptors defaultTileDescriptors;

        //MANUAL ALTERNATIVE to pipelineConfig approach
        //Just for testing for now ... the pipelineConfig approach will
        //potentially be better due to its defines handling
        vsg::ref_ptr<vsg::GraphicsPipeline> pipeline;

    protected:

        //! Creates all the default texture information,
        //! i.e. placeholder textures and uniforms for all tiles
        //! when they don't have actual data.
        void createDefaultDescriptors();

        //! Creates the base shader set used when rendering terrain
        virtual vsg::ref_ptr<vsg::ShaderSet> createShaderSet() const;

        //! Creates a configurator for graphics pipeline state groups.
        //! The configurator does not contain any ACTUAL decriptors (like
        //! textures and uniforms) but rather just prepares the ShaderSet
        //! to work with the specific decriptors you PLAN to provide.
        virtual vsg::ref_ptr<vsg::GraphicsPipelineConfig> createPipelineConfig(vsg::SharedObjects*) const;

        //! ALT MANUAL ALTERNATIVE to using the PipelineCOnfig -
        //! for testing only for now
        //virtual vsg::ref_ptr<vsg::GraphicsPipeline> createPipeline(vsg::SharedObjects*) const;

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
            TextureDef colorParent;
            TextureDef elevation;
            TextureDef normal;
        }
        textures;
    };
}
