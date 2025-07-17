/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LineSystem.h"
#include "../PipelineState.h"
#include "../VSGUtils.h"

#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

#include <vsg/nodes/CullNode.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/StateGroup.h>

#include <cstring>

using namespace ROCKY_NAMESPACE;

#define LINE_VERT_SHADER "shaders/rocky.line.vert"
#define LINE_FRAG_SHADER "shaders/rocky.line.frag"

#define LINE_BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define LINE_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createLineShaderSet(VSGContext& runtime)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(LINE_VERT_SHADER, runtime->searchPaths),
            runtime->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(LINE_FRAG_SHADER, runtime->searchPaths),
            runtime->readerWriterOptions);

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
        shaderSet->addDescriptorBinding("line", "", LINE_BUFFER_SET, LINE_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

LineSystemNode::LineSystemNode(Registry& registry) :
    Inherit(registry)
{
    //nop
}

void
LineSystemNode::initialize(VSGContext& runtime)
{
    // Now create the pipeline and stategroup to bind it
    auto shaderSet = createLineShaderSet(runtime);

    if (!shaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Line shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    pipelines.resize(NUM_PIPELINES);

    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        c.config->shaderHints = runtime->shaderCompileSettings;

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_vertex_prev", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_vertex_next", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);

        // Uniforms we will need:
        c.config->enableDescriptor("line");

        // always both
        PipelineUtils::enableViewDependentData(c.config);

        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::RasterizationState& state) override {
                state.cullMode = VK_CULL_MODE_NONE;
            }
            void apply(vsg::DepthStencilState& state) override {
                if ((feature_mask & WRITE_DEPTH) == 0) {
                    state.depthWriteEnable = (feature_mask & WRITE_DEPTH) ? VK_TRUE : VK_FALSE;
                }
            }
            void apply(vsg::ColorBlendState& state) override {
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments
                {
                    { true,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
                };
            }
        };
        SetPipelineStates visitor(feature_mask);
        c.config->accept(visitor);

        c.config->init();

        // Assemble the commands required to activate this pipeline:
        c.commands = vsg::Commands::create();
        c.commands->children.push_back(c.config->bindGraphicsPipeline);
        c.commands->children.push_back(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, c.config->layout, VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));
    }
}

void
LineSystemNode::createOrUpdateNode(Line& line, detail::BuildInfo& data, VSGContext& runtime) const
{
    if (!data.existing_node)
    {
        auto bindCommand = BindLineDescriptors::create();
        bindCommand->updateStyle(line.style);
        bindCommand->init(getPipelineLayout(line));

        auto stategroup = vsg::StateGroup::create();
        stategroup->stateCommands.push_back(bindCommand);

        vsg::ref_ptr<LineGeometry> geometry;
        vsg::ref_ptr<vsg::Node> geom_root;
        vsg::dmat4 localizer_matrix;

        if (line.referencePoint.valid())
        {
            SRSOperation xform;
            vsg::dvec3 offset, temp;
            std::vector<vsg::vec3> verts32;
            parseReferencePoint(line.referencePoint, xform, offset);

            verts32.reserve(line.points.size());
            for (auto& point : line.points)
            {
                xform(point, temp);
                temp -= offset;
                verts32.emplace_back(temp);
            }

            geometry = LineGeometry::create();
            geometry->set(verts32, line.topology, line.staticSize);

            localizer_matrix = vsg::translate(offset);
            auto localizer = vsg::MatrixTransform::create(localizer_matrix);
            localizer->addChild(geometry);
            geom_root = localizer;
        }
        else
        {
            // no reference point -- push raw geometry
            geometry = LineGeometry::create();
            geometry->set(line.points, line.topology, line.staticSize);
            geom_root = geometry;
        }

        auto cull = vsg::CullNode::create();
        if (stategroup)
        {
            stategroup->addChild(geom_root);
            cull->child = stategroup;
        }
        else
        {
            cull->child = geom_root;
        }

        // hand-calculate the bounding sphere
        geometry->calcBound(cull->bound, localizer_matrix);

        data.new_node = cull;
    }

    else // existing node -- update:
    {
        // style changed?
        if (line.styleDirty)
        {
            auto* bindStyle = util::find<BindLineDescriptors>(data.existing_node);
            if (bindStyle)
            {
                bindStyle->updateStyle(line.style);
            }
        }

        // geometry changed?
        if (line.pointsDirty)
        {
            auto* geometry = util::find<LineGeometry>(data.existing_node);
            if (geometry)
            {
                vsg::dsphere bound;
                vsg::dmat4 localizer_matrix;

                if (line.referencePoint.valid())
                {
                    SRSOperation xform;
                    vsg::dvec3 offset, temp;
                    std::vector<vsg::vec3> verts32;
                    parseReferencePoint(line.referencePoint, xform, offset);

                    verts32.reserve(line.points.size());
                    for (auto& point : line.points)
                    {
                        xform(point, temp);
                        temp -= offset;
                        verts32.emplace_back(temp);
                    }
                    
                    geometry->set(verts32, line.topology, line.staticSize);

                    auto mt = util::find<vsg::MatrixTransform>(data.existing_node);
                    mt->matrix = vsg::translate(offset);
                    localizer_matrix = mt->matrix;
                }
                else
                {
                    // no reference point -- push raw geometry
                    geometry->set(line.points, line.topology, line.staticSize);
                }

                // hand-calculate the bounding sphere
                auto cull = util::find<vsg::CullNode>(data.existing_node);
                geometry->calcBound(cull->bound, localizer_matrix);
            }
        }
    }

    line.styleDirty = false;
    line.pointsDirty = false;
}

int
LineSystemNode::featureMask(const Line& c) const
{
    int mask = 0;
    if (c.write_depth) mask |= WRITE_DEPTH;
    return mask;
}


BindLineDescriptors::BindLineDescriptors()
{
    //nop
}

void
BindLineDescriptors::updateStyle(const LineStyle& value)
{
    bool force = false;

    if (!_styleData)
    {
        _styleData = vsg::ubyteArray::create(sizeof(LineStyle));

        // tells VSG that the contents can change, and if they do, the data should be
        // transfered to the GPU before or during recording.
        _styleData->properties.dataVariance = vsg::DYNAMIC_DATA;

        force = true;
    }

    LineStyle& my_style = *static_cast<LineStyle*>(_styleData->dataPointer());

    if (force || (std::memcmp(&my_style, &value, sizeof(LineStyle)) != 0))
    {
        my_style = value;
        _styleData->dirty();
    }
}

void
BindLineDescriptors::init(vsg::ref_ptr<vsg::PipelineLayout> layout)
{
    vsg::Descriptors descriptors;

    // the style buffer:
    auto ubo = vsg::DescriptorBuffer::create(_styleData, LINE_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptors.push_back(ubo);

    if (!descriptors.empty())
    {
        this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        this->firstSet = 0;
        this->layout = layout;
        this->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(), descriptors);
    }
}


LineGeometry::LineGeometry()
{
    _drawCommand = vsg::DrawIndexed::create(
        0, // index count
        1, // instance count
        0, // first index
        0, // vertex offset
        0  // first instance
    );

    commands.push_back(_drawCommand);
}

void
LineGeometry::setFirst(unsigned value)
{
    _drawCommand->firstIndex = value * 4;
}

void
LineGeometry::setCount(unsigned value)
{
    _drawCommand->indexCount = value * 6;
}

void
LineGeometry::calcBound(vsg::dsphere& output, const vsg::dmat4& matrix) const
{
    int first = _drawCommand->firstIndex / 4;
    int count = _drawCommand->indexCount / 6;

    output.reset();
    for(int i = first; i < count; ++i)
    {
        expandBy(output, matrix * vsg::dvec3(_current->at(i * 4)));
    }
}


void
Line::recycle(entt::registry& registry)
{
    auto& renderable = registry.get<detail::Renderable>(attach_point);
    if (renderable.node)
    {
        auto geometry = util::find<LineGeometry>(renderable.node);
        if (geometry)
        {
            geometry->setCount(0);
        }
    }

    points.clear();
    dirty();
}
