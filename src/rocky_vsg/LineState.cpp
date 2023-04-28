/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LineState.h"
#include "Runtime.h"

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_ENGINE_NAMESPACE;

#define LINE_VERT_SHADER "shaders/rocky.line.vert"
#define LINE_FRAG_SHADER "shaders/rocky.line.frag"
#define LINE_BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define LINE_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define VIEWPORT_BUFFER_SET 1 // hard-coded in VSG ViewDependentState
#define VIEWPORT_BUFFER_BINDING 1 // hard-coded in VSG ViewDependentState (set=1)


vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> LineState::pipelineConfig;
vsg::StateGroup::StateCommands LineState::pipelineStateCommands;
rocky::Status LineState::status;

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createLineShaderSet(Runtime& runtime)
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
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
        shaderSet->addAttributeBinding("in_vertex_prev", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_vertex_next", "", 2, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, {});

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addUniformBinding("line", "", LINE_BUFFER_SET, LINE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // VSG viewport state
        shaderSet->addUniformBinding("vsg_viewports", "", VIEWPORT_BUFFER_SET, VIEWPORT_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

LineState::~LineState()
{
    pipelineConfig = nullptr;
    pipelineStateCommands.clear();
}

void
LineState::initialize(Runtime& runtime)
{
    // Now create the pipeline and stategroup to bind it
    if (!pipelineConfig)
    {
        auto shaderSet = createLineShaderSet(runtime);

        if (!shaderSet)
        {
            status = Status(Status::ConfigurationError, "Line shaders are missing or coorrupt. Check ROCKY_FILE_PATH.");
            return;
        }

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        pipelineConfig->shaderHints = runtime.shaderCompileSettings;

        // activate the arrays we intend to use
        pipelineConfig->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        pipelineConfig->enableArray("in_vertex_prev", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        pipelineConfig->enableArray("in_vertex_next", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        pipelineConfig->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);

        // backface culling off ... we may or may not need this.
        pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        // Temporary decriptors that we will use to set up the PipelineConfig.
        vsg::Descriptors descriptors;
        pipelineConfig->assignUniform(descriptors, "line");
        pipelineConfig->assignUniform(descriptors, "vsg_viewports");

        // Alpha blending to support line smoothing
        pipelineConfig->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
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
        pipelineConfig->additionalDescriptorSetLayout =
            runtime.sharedObjects ? runtime.sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>() :
            vsg::ViewDescriptorSetLayout::create();

        // Initialize GraphicsPipeline from the data in the configuration.
        if (runtime.sharedObjects)
            runtime.sharedObjects->share(pipelineConfig, [](auto gpc) { gpc->init(); });
        else
            pipelineConfig->init();
    }

    vsg::StateGroup::StateCommands commands;

    commands.push_back(pipelineConfig->bindGraphicsPipeline);

    // assign any custom ArrayState that may be required
    //stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(defines);

    // This binds the view-dependent state from VSG (lights, viewport, etc.)
    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, 1);
    commands.push_back(bindViewDescriptorSets);

    if (runtime.sharedObjects)
        runtime.sharedObjects->share(bindViewDescriptorSets);

    pipelineStateCommands = commands;
}




BindLineStyle::BindLineStyle()
{
    ROCKY_HARD_ASSERT(LineState::pipelineConfig, "Did you call LineState::initialize()?");

    _styleData = vsg::ubyteArray::create(sizeof(LineStyle));

    // tells VSG that the contents can change, and if they do, the data should be
    // transfered to the GPU before or during recording.
    _styleData->properties.dataVariance = vsg::DYNAMIC_DATA;

    auto ubo = vsg::DescriptorBuffer::create(_styleData, LINE_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    this->firstSet = 0;

    this->layout = LineState::pipelineConfig->layout;

    this->descriptorSet = vsg::DescriptorSet::create(
        LineState::pipelineConfig->layout->setLayouts.front(),
        vsg::Descriptors{ ubo });

    setStyle(LineStyle{});
}

void
BindLineStyle::setStyle(const LineStyle& value)
{
    LineStyle& my_style = *static_cast<LineStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
}

const LineStyle&
BindLineStyle::style() const
{
    return *static_cast<LineStyle*>(_styleData->dataPointer());
}



LineStringGeometry::LineStringGeometry()
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
LineStringGeometry::setFirst(unsigned value)
{
    _drawCommand->firstIndex = value * 4;
}

void
LineStringGeometry::setCount(unsigned value)
{
    _drawCommand->indexCount = value;
}

unsigned
LineStringGeometry::numVerts() const
{
    return _current.size() / 4;
}

void
LineStringGeometry::push_back(const vsg::vec3& value)
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
LineStringGeometry::compile(vsg::Context& context)
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
        (*indices)[i++] = e + 0; // provoking vertex
        (*indices)[i++] = e + 2;
        (*indices)[i++] = e + 3;
        (*indices)[i++] = e + 0; // provoking vertex
    }

    assignArrays({ vert_array, prev_array, next_array, colors_array });
    assignIndices(indices);

    _drawCommand->indexCount = indices->size();

    commands.clear();
    commands.push_back(_drawCommand);

    vsg::Geometry::compile(context);
}