
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MeshState.h"
#include "Runtime.h"
#include "../Mesh.h" // for MeshStyle

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

using namespace ROCKY_NAMESPACE;

#define MESH_VERT_SHADER "shaders/rocky.mesh.vert"
#define MESH_FRAG_SHADER "shaders/rocky.mesh.frag"
#define MESH_BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define MESH_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define VIEWPORT_BUFFER_SET 1 // hard-coded in VSG ViewDependentState
#define VIEWPORT_BUFFER_BINDING 1 // hard-coded in VSG ViewDependentState (set=1)

//TODO: this can't be a define, it needs to be an option.
//#define SUPPORTS_TEXTURE
#define TEXTURE_SET 0
#define TEXTURE_BINDING 6

//vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> MeshState::pipelineConfig;
//vsg::StateGroup::StateCommands MeshState::pipelineStateCommands;

rocky::Status MeshState::status;
vsg::ref_ptr<vsg::ShaderSet> MeshState::shaderSet;
rocky::Runtime* MeshState::runtime = nullptr;
std::vector<MeshState::Config> MeshState::configs(4); // number of permutations

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
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
        shaderSet->addAttributeBinding("in_normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color", "", 2, VK_FORMAT_R32G32B32A32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_uv", "", 3, VK_FORMAT_R32G32_SFLOAT, {});

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addUniformBinding("mesh", "", MESH_BUFFER_SET, MESH_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Optional texture
        shaderSet->addUniformBinding("mesh_texture", "USE_MESH_TEXTURE", TEXTURE_SET, TEXTURE_BINDING, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // VSG viewport state
        shaderSet->addUniformBinding("vsg_viewports", "", VIEWPORT_BUFFER_SET, VIEWPORT_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});

        shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEWPORT_BUFFER_SET));

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

MeshState::~MeshState()
{
    shaderSet = nullptr;
    runtime = nullptr;
    configs.clear();
}

void
MeshState::initialize(Runtime& in_runtime)
{
    runtime = &in_runtime;
    shaderSet = createShaderSet(in_runtime);

    if (!shaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Mesh shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }
}

MeshState::Config&
MeshState::get(int which)
{
    Config& c = configs[which];

    if (!status.ok())
        return c;

    // Now create the pipeline and stategroup to bind it
    if (!c.pipelineConfig)
    {
        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        c.pipelineConfig->shaderHints = runtime->shaderCompileSettings;

        // activate the arrays we intend to use
        c.pipelineConfig->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.pipelineConfig->enableArray("in_normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.pipelineConfig->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        c.pipelineConfig->enableArray("in_uv", VK_VERTEX_INPUT_RATE_VERTEX, 8);

        // backface culling off ... we may or may not need this.
        //pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        c.pipelineConfig->enableUniform("mesh");
        c.pipelineConfig->enableUniform("vsg_viewports");

        if (which & APPLY_TEXTURE)
        {
            c.pipelineConfig->enableTexture("mesh_texture");
            if (!c.pipelineConfig->shaderHints)
                c.pipelineConfig->shaderHints = vsg::ShaderCompileSettings::create();
            c.pipelineConfig->shaderHints->defines.insert("MESH_TEXTURE");
        }

        // Alpha blending to support line smoothing
        c.pipelineConfig->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
            true,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        } };

        // Enable or disable depth writes
        c.pipelineConfig->depthStencilState->depthWriteEnable = (which & WRITE_DEPTH)? VK_TRUE : VK_FALSE;

        // Register the ViewDescriptorSetLayout (for view-dependent state stuff
        // like viewpoint and lights data)
        // The "set" in GLSL's "layout(set=X, binding=Y)" refers to the index of
        // the descriptor set layout within the pipeline layout. Setting the
        // "additional" DSL appends it to the pipline layout, giving it set=1.
#if 0
        c.pipelineConfig->additionalDescriptorSetLayout =
            runtime->sharedObjects ? runtime->sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>() :
            vsg::ViewDescriptorSetLayout::create();
#else
        Log::warn() << "pipelineConfig->additionalDescriptorSetLayout" << std::endl;
#endif

        // Initialize GraphicsPipeline from the data in the configuration.
        if (runtime->sharedObjects)
            runtime->sharedObjects->share(c.pipelineConfig, [](auto gpc) { gpc->init(); });
        else
            c.pipelineConfig->init();
    }

    vsg::StateGroup::StateCommands commands;

    commands.push_back(c.pipelineConfig->bindGraphicsPipeline);

    // assign any custom ArrayState that may be required
    //stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(defines);

    // This binds the view-dependent state from VSG (lights, viewport, etc.)
    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, c.pipelineConfig->layout, 1);
    commands.push_back(bindViewDescriptorSets);

    if (runtime->sharedObjects)
        runtime->sharedObjects->share(bindViewDescriptorSets);

    c.pipelineStateCommands = commands;

    return c;
}




BindMeshStyle::BindMeshStyle()
{
    ROCKY_HARD_ASSERT_STATUS(MeshState::status);

    _styleData = vsg::ubyteArray::create(sizeof(MeshStyle));

    // tells VSG that the contents can change, and if they do, the data should be
    // transfered to the GPU before or during recording.
    _styleData->properties.dataVariance = vsg::DYNAMIC_DATA;

    setStyle(MeshStyle{});

    dirty();
}

void
BindMeshStyle::setStyle(const MeshStyle& value)
{
    MeshStyle& my_style = *static_cast<MeshStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
}

const MeshStyle&
BindMeshStyle::style() const
{
    return *static_cast<MeshStyle*>(_styleData->dataPointer());
}

void
BindMeshStyle::dirty()
{
    vsg::Descriptors descriptors;

    // the style buffer:
    auto ubo = vsg::DescriptorBuffer::create(_styleData, MESH_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptors.push_back(ubo);

    // the texture, if present:
    if (_imageInfo)
    {
        auto texture = vsg::DescriptorImage::create(_imageInfo, TEXTURE_BINDING, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptors.push_back(texture);
    }

    this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    this->firstSet = 0;

    auto config = MeshState::get(_features).pipelineConfig;

    this->layout = config->layout;

    this->descriptorSet = vsg::DescriptorSet::create(
        config->layout->setLayouts.front(),
        descriptors);
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
    const vsg::vec3& v1, const vsg::vec3& v2, const vsg::vec3& v3,
    const vsg::vec2& uv1, const vsg::vec2& uv2, const vsg::vec2& uv3)
{
    auto iter1 = _lut.find(v1);
    index_type i1 = iter1 != _lut.end() ? iter1->second : _verts.size();
    if (i1 == _verts.size())
    {
        _verts.push_back(v1);
        _uvs.push_back(uv1);
        _lut[v1] = i1;
    }

    auto iter2 = _lut.find(v2);
    index_type i2 = iter2 != _lut.end() ? iter2->second : _verts.size();
    if (i2 == _verts.size())
    {
        _verts.push_back(v2);
        _uvs.push_back(uv2);
        _lut[v2] = i2;
    }

    auto iter3 = _lut.find(v3);
    index_type i3 = iter3 != _lut.end() ? iter3->second : _verts.size();
    if (i3 == _verts.size())
    {
        _verts.push_back(v3);
        _uvs.push_back(uv3);
        _lut[v3] = i3;
    }

    _indices.push_back(i1);
    _indices.push_back(i2);
    _indices.push_back(i3);
}

void
MeshGeometry::compile(vsg::Context& context)
{
    commands.clear();

    if (_verts.size() == 0)
        return;

    _normals.assign(_verts.size(), vsg::vec3(0, 0, 1));
    _colors.assign(_verts.size(), vsg::vec4(1, 1, 1, 1));

    auto vert_array = vsg::vec3Array::create(_verts.size(), _verts.data());
    auto normal_array = vsg::vec3Array::create(_normals.size(), _normals.data());
    auto color_array = vsg::vec4Array::create(_colors.size(), _colors.data());
    auto uv_array = vsg::vec2Array::create(_uvs.size(), _uvs.data());
    auto index_array = vsg::ushortArray::create(_indices.size(), _indices.data());

    assignArrays({ vert_array, normal_array, color_array, uv_array });
    assignIndices(index_array);

    _drawCommand->indexCount = index_array->size();

    commands.push_back(_drawCommand);

    vsg::Geometry::compile(context);
}

void
MeshGeometry::record(vsg::CommandBuffer& buf) const
{
    vsg::Geometry::record(buf);
}