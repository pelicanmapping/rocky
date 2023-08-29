
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MeshSystem.h"
#include "Runtime.h"
#include "PipelineState.h"

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

using namespace ROCKY_NAMESPACE;

#define MESH_VERT_SHADER "shaders/rocky.mesh.vert"
#define MESH_FRAG_SHADER "shaders/rocky.mesh.frag"

#define MESH_UNIFORM_SET 0 // must match layout(set=X) in the shader UBO
#define MESH_STYLE_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define MESH_TEXTURE_BINDING 6

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(Runtime& runtime)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(MESH_VERT_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(MESH_FRAG_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex",      "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
        shaderSet->addAttributeBinding("in_normal",      "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color",       "", 2, VK_FORMAT_R32G32B32A32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_uv",          "", 3, VK_FORMAT_R32G32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_depthoffset", "", 4, VK_FORMAT_R32_SFLOAT, {});

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addUniformBinding("mesh", "USE_MESH_STYLE",
            MESH_UNIFORM_SET, MESH_STYLE_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Optional texture
        shaderSet->addUniformBinding("mesh_texture", "USE_MESH_TEXTURE",
            MESH_UNIFORM_SET, MESH_TEXTURE_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

MeshSystem::MeshSystem(entt::registry& registry) :
    vsg::Inherit<ECS::SystemNode,MeshSystem>(registry),
    helper(registry)
{
    //nop
}

void
MeshSystem::initialize(Runtime& runtime)
{
    auto shaderSet = createShaderSet(runtime);

    if (!shaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Mesh shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    helper.pipelines.resize(NUM_PIPELINES);

    // create all pipeline permutations.
    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = helper.pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Compile settings / defines. We need to clone this since it may be
        // different defines for each configuration permutation.
        c.config->shaderHints = runtime.shaderCompileSettings ?
            vsg::ShaderCompileSettings::create(*runtime.shaderCompileSettings) :
            vsg::ShaderCompileSettings::create();

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        c.config->enableArray("in_uv", VK_VERTEX_INPUT_RATE_VERTEX, 8);
        c.config->enableArray("in_depthoffset", VK_VERTEX_INPUT_RATE_VERTEX, 4);

        // backface culling off ... we may or may not need this.
        //pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        //c.pipelineConfig->enableUniform("vsg_viewports");

        if ((feature_mask & WRITE_DEPTH) == 0)
        {
            c.config->depthStencilState->depthWriteEnable = VK_FALSE;
        }

        if (feature_mask & DYNAMIC_STYLE)
        {
            c.config->enableUniform("mesh");
            c.config->shaderHints->defines.insert("USE_MESH_STYLE");
        }

        if (feature_mask & TEXTURE)
        {
            c.config->enableTexture("mesh_texture");
            c.config->shaderHints->defines.insert("USE_MESH_TEXTURE");
        }

        // Alpha blending to support line smoothing
        c.config->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
            true,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        } };

        // Initialize GraphicsPipeline from the data in the configuration.
        c.config->init();

        c.commands = vsg::Commands::create();
        c.commands->addChild(c.config->bindGraphicsPipeline);
    }
}

int MeshSystem::featureMask(const Mesh& mesh)
{
    int feature_set = 0;
    if (mesh.texture) feature_set |= MeshSystem::TEXTURE;
    if (mesh.style.has_value()) feature_set |= MeshSystem::DYNAMIC_STYLE;
    if (mesh.writeDepth) feature_set |= MeshSystem::WRITE_DEPTH;
    return feature_set;
}


BindMeshDescriptors::BindMeshDescriptors()
{
    //nop
}

void
BindMeshDescriptors::updateStyle(const MeshStyle& value)
{
    if (!_styleData)
    {
        _styleData = vsg::ubyteArray::create(sizeof(MeshStyle));

        // tells VSG that the contents can change, and if they do, the data should be
        // transfered to the GPU before or during recording.
        _styleData->properties.dataVariance = vsg::DYNAMIC_DATA;
    }

    MeshStyle& my_style = *static_cast<MeshStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
}

void
BindMeshDescriptors::init(vsg::ref_ptr<vsg::PipelineLayout> layout)
{
    vsg::Descriptors descriptors;

    // the dynamic style buffer, if present:
    if (_styleData)
    {
        // the style buffer:
        auto ubo = vsg::DescriptorBuffer::create(_styleData, MESH_STYLE_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptors.push_back(ubo);
    }

    // the texture, if present:
    if (_imageInfo)
    {
        auto texture = vsg::DescriptorImage::create(_imageInfo, MESH_TEXTURE_BINDING, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptors.push_back(texture);
    }

    if (!descriptors.empty())
    {
        this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        this->firstSet = 0;
        this->layout = layout;
        this->descriptorSet = vsg::DescriptorSet::create(
            layout->setLayouts.front(),
            descriptors);
    }
}


MeshGeometry::MeshGeometry()
{
    _drawCommand = vsg::DrawIndexed::create(
        0, // index count
        1, // instance count
        0, // first index
        0, // vertex offset
        0  // first instance
    );
}

void
MeshGeometry::add(
    const vsg::vec3* verts,
    const vsg::vec2* uvs,
    const vsg::vec4* colors,
    const float* depthoffsets)
{
    for (int v = 0; v < 3; ++v)
    {
        index_type i = _verts.size();
        auto key = std::make_tuple(verts[v], colors[v]);
        auto iter1 = _lut.find(key);
        i = iter1 != _lut.end() ? iter1->second : _verts.size();

        if (i == _verts.size())
        {
            _verts.push_back(verts[v]);
            _uvs.push_back(uvs[v]);
            _colors.push_back(colors[v]);
            _depthoffsets.push_back(depthoffsets[v]);
            _lut[key] = i;
        }

        _indices.push_back(i);
    }
}

void
MeshGeometry::compile(vsg::Context& context)
{
    if (commands.empty())
    {
        if (_verts.size() == 0)
            return;

        if (_normals.empty())
            _normals.assign(_verts.size(), vsg::vec3(0, 0, 1));

        auto vert_array = vsg::vec3Array::create(_verts.size(), _verts.data());
        auto normal_array = vsg::vec3Array::create(_normals.size(), _normals.data());
        auto color_array = vsg::vec4Array::create(_colors.size(), _colors.data());
        auto uv_array = vsg::vec2Array::create(_uvs.size(), _uvs.data());
        auto depthoffset_array = vsg::floatArray::create(_depthoffsets.size(), _depthoffsets.data());
        auto index_array = vsg::uintArray::create(_indices.size(), _indices.data());

        assignArrays({ vert_array, normal_array, color_array, uv_array, depthoffset_array });
        assignIndices(index_array);

        _drawCommand->indexCount = index_array->size();

        commands.push_back(_drawCommand);
    }

    vsg::Geometry::compile(context);
}
