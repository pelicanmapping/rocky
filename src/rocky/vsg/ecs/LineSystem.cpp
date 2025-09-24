/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LineSystem.h"
#include "../PipelineState.h"
#include "../VSGUtils.h"

#include <cstring>

using namespace ROCKY_NAMESPACE;

#define LINE_VERT_SHADER "shaders/rocky.line.vert"
#define LINE_FRAG_SHADER "shaders/rocky.line.frag"

#define LINE_BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define LINE_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createLineShaderSet(VSGContext& vsgcontext)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(LINE_VERT_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(LINE_FRAG_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

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
LineSystemNode::initialize(VSGContext& vsgcontext)
{
    // Now create the pipeline and stategroup to bind it
    auto shaderSet = createLineShaderSet(vsgcontext);

    if (!shaderSet)
    {
        status = Failure(Failure::ResourceUnavailable,
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
        c.config->shaderHints = vsgcontext->shaderCompileSettings;

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
LineSystemNode::createOrUpdateNode(const Line& line, detail::BuildInfo& data, VSGContext& vsgcontext) const
{
    bool reallocate = false;
    bool uploadStyle = false;
    bool uploadPoints = false;

    if (!data.existing_node)
    {
        reallocate = true;
    }
    else
    {
        auto drawable = util::find<LineDrawable>(data.existing_node);
        if (drawable && drawable->_capacity < line.geometry._capacity)
        {
            reallocate = true;
        }
    }

    if (reallocate)
    {
        if (data.existing_node)
        {
            vsgcontext->dispose(data.existing_node);
        }

        auto bindCommand = BindLineDescriptors::create();
        bindCommand->updateStyle(line.style);
        bindCommand->init(getPipelineLayout(line));

        auto stategroup = vsg::StateGroup::create();
        stategroup->stateCommands.push_back(bindCommand);

        vsg::ref_ptr<LineDrawable> drawable;
        vsg::ref_ptr<vsg::Node> geometry_root;
        vsg::dmat4 localizer_matrix;
        auto& geom = line.geometry;

        if (geom.srs.valid())
        {
            GeoPoint anchor(geom.srs, 0, 0);
            if (!geom.points.empty())
                anchor = GeoPoint(geom.srs, (geom.points.front() + geom.points.back()) * 0.5);

            SRSOperation xform;
            glm::dvec3 offset;
            bool ok = parseReferencePoint(anchor, xform, offset);
            ROCKY_SOFT_ASSERT_AND_RETURN(ok, void());

            // make a copy that we will use to transform and offset:
            drawable = LineDrawable::create();
            if (!geom.points.empty())
            {
                std::vector<glm::dvec3> copy(geom.points);
                xform.transformRange(copy.begin(), copy.end());
                for (auto& point : copy)
                    point -= offset;
                drawable->set(copy, geom.topology, geom._capacity);
            }
            else
            {
                drawable->set(geom.points, geom.topology, geom._capacity);
            }


            localizer_matrix = vsg::translate(to_vsg(offset));
            auto localizer = vsg::MatrixTransform::create(localizer_matrix);
            localizer->addChild(drawable);
            geometry_root = localizer;
        }
        else
        {
            // no reference point -- push raw geometry
            drawable = LineDrawable::create();
            drawable->set(geom.points, geom.topology, geom._capacity);
            geometry_root = drawable;
        }

        auto cull = vsg::CullNode::create();
        if (stategroup)
        {
            stategroup->addChild(geometry_root);
            cull->child = stategroup;
        }
        else
        {
            cull->child = geometry_root;
        }

        // hand-calculate the bounding sphere
        drawable->calcBound(cull->bound, localizer_matrix);

        data.new_node = cull;

        uploadStyle = true;
        uploadPoints = true;
    }

    else // existing node -- update:
    {
        // style:
        auto* bindStyle = util::find<BindLineDescriptors>(data.existing_node);
        if (bindStyle)
        {
            uploadStyle = bindStyle->updateStyle(line.style);
        }

        // points:
        auto* drawable = util::find<LineDrawable>(data.existing_node);
        if (drawable)
        {
            vsg::dsphere bound;
            vsg::dmat4 localizer_matrix;

            auto& geom = line.geometry;

            if (geom.srs.valid() && geom.points.size() > 0)
            {
                GeoPoint anchor(geom.srs, (geom.points.front() + geom.points.back()) * 0.5);

                SRSOperation xform;
                glm::dvec3 offset;
                bool ok = parseReferencePoint(anchor, xform, offset);
                ROCKY_SOFT_ASSERT_AND_RETURN(ok, void());

                // make a copy that we will use to transform and offset:
                std::vector<glm::dvec3> copy(geom.points);
                xform.transformRange(copy.begin(), copy.end());
                for (auto& point : copy)
                    point -= offset;

                drawable->set(copy, geom.topology, geom._capacity);

                auto mt = util::find<vsg::MatrixTransform>(data.existing_node);
                mt->matrix = vsg::translate(to_vsg(offset));
                localizer_matrix = mt->matrix;
            }
            else
            {
                // no reference point -- push raw geometry
                drawable->set(geom.points, geom.topology, geom._capacity);
            }

            // hand-calculate the bounding sphere
            auto cull = util::find<vsg::CullNode>(data.existing_node);
            drawable->calcBound(cull->bound, localizer_matrix);

            uploadPoints = true;
        }
    }

    // check for updates to the style, and upload the new data if needed:
    if (uploadStyle)
    {
        auto stategroup = util::find<vsg::StateGroup>(data.new_node ? data.new_node : data.existing_node);
        if (stategroup)
        {
            auto bindCommand = stategroup->stateCommands[0]->cast<BindLineDescriptors>();
            vsgcontext->upload(bindCommand->_ubo->bufferInfoList);
        }
    }

    if (uploadPoints)
    {
        auto drawable = util::find<LineDrawable>(data.new_node ? data.new_node : data.existing_node);
        if (drawable)
        {
            if (drawable->_current && drawable->indices)
            {
                vsgcontext->upload(drawable->arrays);
                vsgcontext->upload(vsg::BufferInfoList{ drawable->indices });
            }
        }
    }
}

int
LineSystemNode::featureMask(const Line& c) const
{
    int mask = 0;
    if (c.writeDepth) mask |= WRITE_DEPTH;
    return mask;
}


BindLineDescriptors::BindLineDescriptors()
{
    //nop
}

bool
BindLineDescriptors::updateStyle(const LineStyle& value)
{
    bool force = false;

    if (!_styleData)
    {
        _styleData = vsg::ubyteArray::create(sizeof(LineStyle));
        // do NOT mark as DYNAMIC_DATA, since we only update it when the style changes.
        force = true;
    }

    LineStyle& my_style = *static_cast<LineStyle*>(_styleData->dataPointer());

    if (force || (std::memcmp(&my_style, &value, sizeof(LineStyle)) != 0))
    {
        my_style = value;
        _styleData->dirty();
        return true;
    }

    return false;
}

void
BindLineDescriptors::init(vsg::ref_ptr<vsg::PipelineLayout> layout)
{
    vsg::Descriptors descriptors;

    // the style buffer:
    _ubo = vsg::DescriptorBuffer::create(_styleData, LINE_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptors.push_back(_ubo);

    if (!descriptors.empty())
    {
        this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        this->firstSet = 0;
        this->layout = layout;
        this->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(), descriptors);
    }
}


LineDrawable::LineDrawable()
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
LineDrawable::setFirst(unsigned value)
{
    _drawCommand->firstIndex = value * 4;
}

void
LineDrawable::setCount(unsigned value)
{
    _drawCommand->indexCount = value * 6;
}

void
LineDrawable::calcBound(vsg::dsphere& output, const vsg::dmat4& matrix) const
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
        auto geometry = util::find<LineDrawable>(renderable.node);
        if (geometry)
        {
            geometry->setCount(0);
        }
    }

    geometry.points.clear();
    dirty();
}
