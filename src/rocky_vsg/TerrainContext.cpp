/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainContext.h"
#include <rocky/Notify.h>

#include <vsg/state/ShaderStage.h>

using namespace rocky;


TerrainContext::TerrainContext(RuntimeContext& rt, const Config& conf) :
    TerrainSettings(conf),
    runtime(rt)
{
    terrainShaderSet = createTerrainShaderSet();
}

vsg::ref_ptr<vsg::ShaderSet>
TerrainContext::createTerrainShaderSet() const
{ 
    vsg::ref_ptr<vsg::ShaderSet> shaderSet;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    searchPaths.push_back(vsg::Path("H:/devel/rocky/install/share"));

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vsg::findFile("terrain.vert", searchPaths));

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile("terrain.frag", searchPaths));

    ROCKY_SOFT_ASSERT_AND_RETURN(
        vertexShader && fragmentShader,
        shaderSet,
        "Could not create terrain shaders");

    // TODO
#if 1
    const uint32_t reverseDepth = 0;
    const uint32_t numLayers = 0;
    vsg::ShaderStage::SpecializationConstants specializationConstants
    {
        {0, vsg::uintValue::create(reverseDepth)},
        {1, vsg::uintValue::create(numLayers)}
    };
    vertexShader->specializationConstants = specializationConstants;
    fragmentShader->specializationConstants = specializationConstants;
#endif

    vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

    shaderSet = vsg::ShaderSet::create(shaderStages);

    shaderSet->addAttributeBinding("inVertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNormal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inUV", "", 2, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNeighborVertex", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNeighborNormal", "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    //shaderSet->addUniformBinding("displacementMap", "VSG_DISPLACEMENT_MAP", 0, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1, 1));
    //shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    //shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    //shaderSet->optionalDefines = { "VSG_POINT_SPRITE", "VSG_GREYSACLE_DIFFUSE_MAP" };

    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create() });
    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create() });
    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create() });

    return shaderSet;
}
