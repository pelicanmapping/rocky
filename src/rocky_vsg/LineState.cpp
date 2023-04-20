/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LineState.h"
#include "RuntimeContext.h"

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

using namespace ROCKY_NAMESPACE;

#define LINE_VERT_SHADER "rocky.line.vert"
#define LINE_FRAG_SHADER "rocky.line.frag"
#define LINE_BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define LINE_BUFFER_BINDING 13 // must match the layout(binding=X) in the shader UBO
#define VIEWPORT_BUFFER_SET 1 // hard-coded in VSG ViewDependentState
#define VIEWPORT_BUFFER_BINDING 1 // hard-coded in VSG ViewDependentState

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createLineShaderSet(RuntimeContext& runtime)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(LINE_VERT_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(LINE_FRAG_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { }); // vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("in_vertex_prev", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {}); // vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("in_vertex_next", "", 2, VK_FORMAT_R32G32B32_SFLOAT, {}); // vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("in_color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, {}); // vsg::vec4Array::create(1));

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addUniformBinding("line", "", LINE_BUFFER_SET, LINE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // VSG viewport state
        shaderSet->addUniformBinding("vsg_viewports", "", VIEWPORT_BUFFER_SET, VIEWPORT_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {}); // vsg::vec4Array::create(64));

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}


LineStateFactory::LineStateFactory(RuntimeContext& runtime) :
    _runtime(runtime)
{
    auto shaderSet = createLineShaderSet(runtime);

    if (!shaderSet)
    {
        //todo
        status = Status(Status::ConfigurationError, "Unable to create shader set - check for missing shaders");
        return;
    }

    // Create the pipeline configurator for terrain; this is a helper object
    // that acts as a "template" for terrain tile rendering state.
    _config = vsg::GraphicsPipelineConfig::create(shaderSet);

    // Apply any custom compile settings / defines:
    _config->shaderHints = runtime.shaderCompileSettings;

    // activate the arrays we intend to use
    _config->enableArray("in_vertex"     , VK_VERTEX_INPUT_RATE_VERTEX, 12);
    _config->enableArray("in_vertex_prev", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    _config->enableArray("in_vertex_next", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    _config->enableArray("in_color"      , VK_VERTEX_INPUT_RATE_VERTEX, 16);

    // backface culling off ... we may or may not need this.
    _config->rasterizationState->cullMode = VK_CULL_MODE_NONE;

    // Temporary decriptors that we will use to set up the PipelineConfig.
    vsg::Descriptors descriptors;
    _config->assignUniform(descriptors, "line", { });
    _config->assignUniform(descriptors, "vsg_viewports", { });

    // Alpha blending to support line smoothing
    _config->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
        true,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    } };

    // Register the ViewDescriptorSetLayout (for view-dependent state stuff
    // like viewpoint and lights data)
    // The "set" in GLSL's "layout(set=X, binding=Y)" refers to the index of
    // the descriptor set layout within the pipeline layout. Setting the
    // "additional" DSL appends it to the pipline layout, giving it set=1.
    _config->additionalDescriptorSetLayout =
        runtime.sharedObjects ? runtime.sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>() :
        vsg::ViewDescriptorSetLayout::create();

    // Initialize GraphicsPipeline from the data in the configuration.
    if (runtime.sharedObjects)
        runtime.sharedObjects->share(_config, [](auto gpc) { gpc->init(); });
    else
        _config->init();
}

vsg::StateGroup::StateCommands
LineStateFactory::createPipelineStateCommands() const
{
    // Now create the pipeline and stategroup to bind it
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), { });

    vsg::StateGroup::StateCommands commands;

    commands.push_back(_config->bindGraphicsPipeline);

    // assign any custom ArrayState that may be required
    //stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(defines);

    // This binds the view-dependent state from VSG (lights, viewport, etc.)
    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _config->layout, 1);
    commands.push_back(bindViewDescriptorSets);

    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(bindViewDescriptorSets);

    return commands;
}

vsg::ref_ptr<vsg::StateCommand>
LineStateFactory::createBindDescriptorSetCommand(const LineStyle& style) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), { });

    // assemble the uniform buffer object:
    LineStyleDescriptors::Uniforms uniforms;
    uniforms.first = 0;
    uniforms.last = -1;
    uniforms.color = style.color;
    uniforms.width = style.width;
    uniforms.stipple_factor = style.stipple_factor;
    uniforms.stipple_pattern = style.stipple_pattern;

    // populate the buffer:
    vsg::ref_ptr<vsg::ubyteArray> data = vsg::ubyteArray::create(sizeof(uniforms));
    memcpy(data->dataPointer(), &uniforms, sizeof(uniforms));
    auto ubo = vsg::DescriptorBuffer::create(data, LINE_BUFFER_BINDING);

    // assign it to a dset:
    auto dset = vsg::DescriptorSet::create(
        _config->layout->setLayouts.front(), // layout this dset complies with
        vsg::Descriptors{ ubo }              // dset contents
    );
    
    // line styles seem likely to be shared.
    _runtime.sharedObjects->share(dset);

    // make the bind command. this will parent any actual line geometry commands
    // that should use the style.
    auto bind = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        _config->layout,
        0, // first set
        dset
    );

    return bind;
}



LineStringStyleNode::LineStringStyleNode(RuntimeContext& runtime) :
    _runtime(runtime)
{
    //nop
}

void
LineStringStyleNode::setStyle(const LineStyle& style)
{
    _style = style;
    _runtime.dirty(this);
}

void
LineStringStyleNode::compile(vsg::Context& context)
{
    auto bind = _runtime.lineState().createBindDescriptorSetCommand(_style);
    this->stateCommands.clear();
    this->add(bind);
    vsg::StateGroup::compile(context);
}



LineStringNode::LineStringNode()
{
    //nop
}

unsigned
LineStringNode::numVerts() const
{
    return _current.size() / 4;
}

void
LineStringNode::push_back(const vsg::vec3& value)
{
    bool first = _current.empty();

    _previous.push_back(first ? value : _current.back());
    _previous.push_back(first ? value : _current.back());
    _previous.push_back(first ? value : _current.back());
    _previous.push_back(first ? value : _current.back());

    if (!first)
    {
        *(_next.end() - 4) = value;
        *(_next.end() - 3) = value;
        *(_next.end() - 2) = value;
        *(_next.end() - 1) = value;
    }

    _current.push_back(value);
    _current.push_back(value);
    _current.push_back(value);
    _current.push_back(value);

    _next.push_back(value);
    _next.push_back(value);
    _next.push_back(value);
    _next.push_back(value);

    _colors.push_back(_defaultColor);
    _colors.push_back(_defaultColor);
    _colors.push_back(_defaultColor);
    _colors.push_back(_defaultColor);
}

void
LineStringNode::compile(vsg::Context& context)
{
    if (_current.size() == 0)
        return;

    auto vert_array = vsg::vec3Array::create(_current.size(), _current.data());
    auto prev_array = vsg::vec3Array::create(_previous.size(), _previous.data());
    auto next_array = vsg::vec3Array::create(_next.size(), _next.data());
    auto colors_array = vsg::vec4Array::create(_colors.size(), _colors.data());

    unsigned numIndices = (numVerts() - 1) * 6;
    auto indices = vsg::ushortArray::create(numIndices);
    for (int e = 2, i = 0; e < _current.size() - 2; e += 4)
    {
        (*indices)[i++] = e + 3;
        (*indices)[i++] = e + 1;
        (*indices)[i++] = e + 0; // Provoking vertex
        (*indices)[i++] = e + 2;
        (*indices)[i++] = e + 3;
        (*indices)[i++] = e + 0; // provoking vertex
    }

    assignArrays({ vert_array, prev_array, next_array, colors_array });
    assignIndices(indices);

    commands.push_back(
        vsg::DrawIndexed::create(
            indices->size(), // index count
            1,               // instance count
            0,               // first index
            0,               // vertex offset
            0));             // first instance

    vsg::Geometry::compile(context);
}
