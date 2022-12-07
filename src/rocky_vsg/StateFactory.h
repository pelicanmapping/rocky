/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>

#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfig.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/SharedObjects.h>
#include <vsg/nodes/StateGroup.h>
#include <rocky_vsg/TileRenderModel.h>

namespace rocky
{
    class TerrainTileNode;

    struct TextureBinding
    {
        std::string name;
        uint32_t uniform_binding;
        vsg::ref_ptr<vsg::Sampler> sampler;
        vsg::ref_ptr<vsg::Data> defaultData;
    };

    class ROCKY_VSG_INTERNAL StateFactory
    {
    public:
        //! Initialize the factory
        StateFactory();

        //! Creates a state group for rendering terrain
        virtual vsg::ref_ptr<vsg::StateGroup> createTerrainStateGroup() const;

        //! Creates a state group for rendering a specific terrain tile
        virtual TileDescriptorModel createTileDescriptorModel(const TileRenderModel& renderModel) const;

        //! Updates a tile's state group to reflect its current descriptor model.
        virtual void refreshStateGroup(TerrainTileNode* tile) const;

    public:

        //! Config object for creating the terrain's graphics pipeline
        vsg::ref_ptr<vsg::GraphicsPipelineConfig> pipelineConfig;

        //! VSG parent shader set that we use to develop the terrain tile
        //! state group for each tile.
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        //! Cache of VSG objects shared by different parts of the
        //! terrain rendering subsystem.
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! Default state model for a terrain tile.
        TileDescriptorModel defaultTileDescriptors;

    protected:

        //! Creates the base shader set used when rendering terrain
        virtual vsg::ref_ptr<vsg::ShaderSet> createShaderSet() const;

        //! Creates a configurator for graphics pipeline state groups.
        //! The configurator does not contain any ACTUAL decriptors (like
        //! textures and uniforms) but rather just prepares the ShaderSet
        //! to work with the specific decriptors you PLAN to provide.
        virtual vsg::ref_ptr<vsg::GraphicsPipelineConfig> createPipelineConfig(vsg::SharedObjects*) const;

        //! Creates all the default texture information.
        void createDefaultDescriptors();

        //! stock samplers to use for terrain textures
        TextureBinding _color;
        TextureBinding _colorParent;
        TextureBinding _elevation;
        TextureBinding _normal;

        //vsg::ref_ptr<vsg::Sampler>
        //    _colorSampler,
        //    _elevationSampler,
        //    _normalSampler;
    };
}
