/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LineSystem.h"
#include "../PipelineState.h"
#include "../VSGUtils.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define LINE_VERT_SHADER "shaders/rocky.line.vert"
#define LINE_FRAG_SHADER "shaders/rocky.line.frag"

#define LINE_SET 0
#define LINE_BINDING_STYLE_LUT 0 // layout(set=0, binding=0) in the shader
#define LINE_BINDING_UNIFORMS  1 // layout(set=0, binding=1) in the shader

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
        //shaderSet->addAttributeBinding("in_color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, {});

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addDescriptorBinding("styles", "", LINE_SET, LINE_BINDING_STYLE_LUT,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        shaderSet->addDescriptorBinding("line", "", LINE_SET, LINE_BINDING_UNIFORMS,
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
    _styleInUse.fill(false);
}

namespace
{
    void on_construct_Line(entt::registry& r, entt::entity e)
    {
        r.emplace<LineDetail>(e);

        // TODO: put this in a utility function somewhere
        // common components that may already exist on this entity:
        (void) r.get_or_emplace<ActiveState>(e);
        (void) r.get_or_emplace<Visibility>(e);

        r.get<Line>(e).owner = e;

        // do this last, so that everything is set up when dirty is processed
        r.get<Line>(e).dirty(r);
    }

    void on_construct_LineStyle(entt::registry& r, entt::entity e)
    {
        r.emplace<LineStyleDetail>(e);

        r.get<LineStyle>(e).owner = e;

        // do this last, so that everything is set up when dirty is processed
        r.get<LineStyle>(e).dirty(r);
    }

    void on_construct_LineGeometry(entt::registry& r, entt::entity e)
    {
        r.emplace<LineGeometryDetail>(e);

        r.get<LineGeometry>(e).owner = e;

        // do this last, so that everything is set up when dirty is processed
        r.get<LineGeometry>(e).dirty(r);
    }


    void on_destroy_Line(entt::registry& r, entt::entity e)
    {
        r.remove<LineDetail>(e);
    }

    void on_destroy_LineStyle(entt::registry& r, entt::entity e)
    {
        r.remove<LineStyleDetail>(e);
    }

    void on_destroy_LineGeometry(entt::registry& r, entt::entity e)
    {
        r.remove<LineGeometryDetail>(e);
    }
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

    _pipelines.resize(NUM_PIPELINES);

    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = _pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        c.config->shaderHints = vsgcontext->shaderCompileSettings;

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_vertex_prev", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_vertex_next", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        //c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);

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
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments {
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

    // Style look up table.
    _styleLUT_data = vsg::ubyteArray::create(sizeof(LineStyleLUT));
    _styleLUT_buffer = vsg::DescriptorBuffer::create(_styleLUT_data, LINE_BINDING_STYLE_LUT, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // add a default style in slot 0.
    auto& styleLUT = *static_cast<LineStyleLUT*>(_styleLUT_data->dataPointer());
    styleLUT.lut[0].populate(LineStyle());
    _styleInUse[0] = true;
    _styleLUTSize = 1;

    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks for Lines
            r.on_construct<Line>().connect<&on_construct_Line>();
            r.on_construct<LineStyle>().connect<&on_construct_LineStyle>();
            r.on_construct<LineGeometry>().connect<&on_construct_LineGeometry>();

            r.on_destroy<Line>().connect<&on_destroy_Line>();
            r.on_destroy<LineStyle>().connect<&on_destroy_LineStyle>();
            r.on_destroy<LineGeometry>().connect<&on_destroy_LineGeometry>();

            // Set up the dirty tracking.
            auto e = r.create();
            r.emplace<Line::Dirty>(e);
            r.emplace<LineStyle::Dirty>(e);
            r.emplace<LineGeometry::Dirty>(e);
        });
}

void
LineSystemNode::createOrUpdateLineNode(const Line& line, LineDetail& lineDetail, LineStyleDetail* style, LineGeometryDetail* geom, VSGContext& context)
{
    auto layout = getPipelineLayout(line);

    if (!lineDetail.node)
    {
        auto bind = BindLineDescriptors::create();

        bind->_lineUniforms_data = vsg::ubyteArray::create(sizeof(LineUniforms));
        bind->_lineUniforms_buffer = vsg::DescriptorBuffer::create(bind->_lineUniforms_data, LINE_BINDING_UNIFORMS, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        bind->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        bind->firstSet = 0;
        bind->layout = layout;
        bind->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(),
            vsg::Descriptors{ _styleLUT_buffer, bind->_lineUniforms_buffer });

        lineDetail.node = vsg::StateGroup::create();
        lineDetail.node->stateCommands.emplace_back(bind);
        lineDetail.bind = bind;

        compile(lineDetail.node);
    }

    ROCKY_SOFT_ASSERT_AND_RETURN(lineDetail.node, void());

    // remove the children so we can rebuild the graph.
    lineDetail.node->children.clear();

    vsg::Group* parent = lineDetail.node;

    if (style)
    {
        LineUniforms& uniforms = *static_cast<LineUniforms*>(lineDetail.bind->_lineUniforms_data->dataPointer());
        uniforms.style = style->index;
        upload(lineDetail.bind->_lineUniforms_buffer->bufferInfoList);
    }

    if (geom)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(geom->node, void(), "LineGeometryDetail node is missing");
        parent->addChild(geom->node);
    }
}


void
LineSystemNode::createOrUpdateLineGeometry(const LineGeometry& geom, LineGeometryDetail& geomDetail, VSGContext& vsgcontext)
{
    // NB: registry is read-locked

    bool reallocate = false;

    if (!geomDetail.node)
    {
        reallocate = true;
    }
    else
    {
        if (geomDetail.geomNode && geom.points.capacity() > geomDetail.capacity)
        {
            reallocate = true;
        }
    }

    if (reallocate)
    {
        if (geomDetail.geomNode)
            vsgcontext->dispose(geomDetail.geomNode);

        geomDetail.geomNode = LineGeometryNode::create();

        // update the known capacity:
        geomDetail.capacity = geom.points.capacity();

        vsg::ref_ptr<vsg::Node> root;
        vsg::dmat4 localizer_matrix;

        if (geom.srs.valid())
        {
            GeoPoint anchor(geom.srs, 0, 0);
            if (!geom.points.empty())
                anchor = GeoPoint(geom.srs, (geom.points.front() + geom.points.back()) * 0.5);

            ROCKY_SOFT_ASSERT_AND_RETURN(anchor.valid(), void());
            auto [xform, offset] = anchor.parseAsReferencePoint();

            // make a copy that we will use to transform and offset:
            if (!geom.points.empty())
            {
                std::vector<glm::dvec3> copy(geom.points);
                xform.transformRange(copy.begin(), copy.end());
                for (auto& point : copy)
                    point -= offset;

                geomDetail.geomNode->set(copy, geom.topology, geomDetail.capacity);
            }
            else
            {
                geomDetail.geomNode->set(geom.points, geom.topology, geomDetail.capacity);
            }


            localizer_matrix = vsg::translate(to_vsg(offset));
            auto localizer = vsg::MatrixTransform::create(localizer_matrix);
            localizer->addChild(geomDetail.geomNode);
            root = localizer;
        }
        else
        {
            // no reference point -- push raw geometry
            geomDetail.geomNode->set(geom.points, geom.topology, geomDetail.capacity);
            root = geomDetail.geomNode;
        }

        if (!geomDetail.cullNode)
        {
            geomDetail.cullNode = vsg::CullNode::create();
        }

        geomDetail.cullNode->child = root;

        // hand-calculate the bounding sphere
        geomDetail.geomNode->calcBound(geomDetail.cullNode->bound, localizer_matrix);

        geomDetail.node = geomDetail.cullNode;

        compile(geomDetail.node);
    }

    else // existing node -- update:
    {
        vsg::dsphere bound;
        vsg::dmat4 localizer_matrix;

        if (geom.srs.valid() && geom.points.size() > 0)
        {
            GeoPoint anchor(geom.srs, (geom.points.front() + geom.points.back()) * 0.5);

            ROCKY_SOFT_ASSERT_AND_RETURN(anchor.valid(), void());

            auto [xform, offset] = anchor.parseAsReferencePoint();

            // make a copy that we will use to transform and offset:
            std::vector<glm::dvec3> copy(geom.points);
            xform.transformRange(copy.begin(), copy.end());
            for (auto& point : copy)
                point -= offset;

            geomDetail.geomNode->set(copy, geom.topology, geomDetail.capacity);

            auto mt = util::find<vsg::MatrixTransform>(geomDetail.node);
            mt->matrix = vsg::translate(to_vsg(offset));
            localizer_matrix = mt->matrix;
        }
        else
        {
            // no reference point -- push raw geometry
            geomDetail.geomNode->set(geom.points, geom.topology, geomDetail.capacity);
        }

        // hand-calculate the bounding sphere
        geomDetail.geomNode->calcBound(geomDetail.cullNode->bound, localizer_matrix);

        // upload the changed arrays
        upload(geomDetail.geomNode->arrays);
        upload(geomDetail.geomNode->indices);
    }
}

void
LineSystemNode::createOrUpdateLineStyle(const LineStyle& style, LineStyleDetail& styleDetail)
{
    auto& styleLUT = *static_cast<LineStyleLUT*>(_styleLUT_data->dataPointer());

    if (styleDetail.index >= 0)
    {
        styleLUT.lut[styleDetail.index].populate(style);
    }
    else
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(_styleLUTSize <= MAX_LINE_STYLES, void(),
            "Line style LUT overflow - call support");

        for (unsigned i = 0; i < MAX_LINE_STYLES; ++i)
        {
            if (!_styleInUse[i])
            {
                _styleInUse[i] = true;
                styleLUT.lut[i].populate(style);
                styleDetail.index = i;
                _styleLUTSize = std::max(_styleLUTSize, i + 1);
                break;
            }
        }
    }
}

void
LineSystemNode::traverse(vsg::RecordTraversal& record) const
{
    const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

    detail::RenderingState rs {
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<Line, LineDetail, ActiveState, Visibility>();
            for (auto&& [entity, comp, lineDetail, active, visibility] : view.each())
            {
                if (lineDetail.node && visible(visibility, rs))
                {
                    auto* transformDetail = reg.try_get<TransformDetail>(entity);
                    if (transformDetail)
                    {
                        if (transformDetail->passingCull(rs))
                        {
                            _renderLeaves.emplace_back(RenderLeaf{ lineDetail.node, transformDetail });
                        }
                    }
                    else
                    {
                        _renderLeaves.emplace_back(RenderLeaf{ lineDetail.node, nullptr });
                    }
                }
            }
        });

    // Render collected data
    if (!_renderLeaves.empty())
    {
        _pipelines[0].commands->accept(record);

        for (auto& leaf : _renderLeaves)
        {
            if (leaf.xformDetail)
            {
                leaf.xformDetail->push(record);
            }

            leaf.node->accept(record);

            if (leaf.xformDetail)
            {
                leaf.xformDetail->pop(record);
            }
        }
    }

    _renderLeaves.clear();
}

void
LineSystemNode::update(VSGContext& vsgcontext)
{
    bool uploadStyles = false;

    _registry.read([&](entt::registry& reg)
        {
            LineStyle::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [style, styleDetail] = reg.get<LineStyle, LineStyleDetail>(e);
                    createOrUpdateLineStyle(style, styleDetail);
                    uploadStyles = true;
                });

            LineGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [geom, geomDetail] = reg.get<LineGeometry, LineGeometryDetail>(e);
                    createOrUpdateLineGeometry(geom, geomDetail, vsgcontext);
                });

            Line::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [line, lineDetail] = reg.get<Line, LineDetail>(e);
                    auto* styleDetail = line.style != entt::null ? reg.try_get<LineStyleDetail>(line.style) : nullptr;
                    auto* geomDetail = line.geometry != entt::null ? reg.try_get<LineGeometryDetail>(line.geometry) : nullptr;
                    createOrUpdateLineNode(line, lineDetail, styleDetail, geomDetail, vsgcontext);
                });                    
        });

    if (uploadStyles)
    {
        upload(_styleLUT_buffer->bufferInfoList);
    }

    Inherit::update(vsgcontext);
}

LineGeometryNode::LineGeometryNode()
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
LineGeometryNode::setFirst(unsigned value)
{
    _drawCommand->firstIndex = value * 4;
}

void
LineGeometryNode::setCount(unsigned value)
{
    _drawCommand->indexCount = value * 6;
}

void
LineGeometryNode::calcBound(vsg::dsphere& output, const vsg::dmat4& matrix) const
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
LineGeometry::recycle(entt::registry& reg)
{
    auto& geomDetail = reg.get<LineGeometryDetail>(owner);
    if (geomDetail.node)
    {
        auto geom = util::find<LineGeometryNode>(geomDetail.node);
        if (geom)
            geom->setCount(0);
    }
    points.clear();
    dirty(reg);
}
