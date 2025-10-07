/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
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
#define LINE_BINDING_UNIFORM  1 // layout(set=0, binding=1) in the shader

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

        shaderSet->addDescriptorBinding("line", "", LINE_SET, LINE_BINDING_UNIFORM,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }


    // creates an empty, default style detail bind command, ready to be populated.
    void initializeStyleDetail(vsg::PipelineLayout* layout, LineStyleDetail& styleDetail)
    {
        // uniform: "mesh.styles" in the shader
        styleDetail.styleData = vsg::ubyteArray::create(sizeof(LineStyleUniform));
        styleDetail.styleUBO = vsg::DescriptorBuffer::create(styleDetail.styleData, LINE_BINDING_UNIFORM, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // bind command:
        styleDetail.bind = vsg::BindDescriptorSet::create();
        styleDetail.bind->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        styleDetail.bind->firstSet = 0;
        styleDetail.bind->layout = layout;
        styleDetail.bind->descriptorSet = vsg::DescriptorSet::create(
            styleDetail.bind->layout->setLayouts.front(),
            vsg::Descriptors{ styleDetail.styleUBO });

        auto& uniforms = *static_cast<LineStyleUniform*>(styleDetail.styleData->dataPointer());
        uniforms.style = LineStyleRecord(); // default style
    }
}

LineSystemNode::LineSystemNode(Registry& registry) :
    Inherit(registry)
{
    //nop
}

namespace
{
    // disposal vector processed by the system
    static std::mutex s_cleanupMutex;
    static vsg::ref_ptr<vsg::Objects> s_toDispose = vsg::Objects::create();
    static inline void dispose(vsg::Object* object) {
        if (object) {
            std::scoped_lock lock(s_cleanupMutex);
            s_toDispose->addChild(vsg::ref_ptr<vsg::Object>(object));
        }
    }

    void on_construct_Line(entt::registry& r, entt::entity e)
    {
        r.emplace<LineDetail>(e);

        // TODO: put this in a utility function somewhere
        // common components that may already exist on this entity:
        (void) r.get_or_emplace<ActiveState>(e);
        (void) r.get_or_emplace<Visibility>(e);

        r.get<Line>(e).owner = e;
        r.get<Line>(e).dirty(r);
    }
    void on_construct_LineStyle(entt::registry& r, entt::entity e)
    {
        r.emplace<LineStyleDetail>(e);
        r.get<LineStyle>(e).owner = e;
        r.get<LineStyle>(e).dirty(r);
    }
    void on_construct_LineGeometry(entt::registry& r, entt::entity e)
    {
        r.emplace<LineGeometryDetail>(e);
        r.get<LineGeometry>(e).owner = e;
        r.get<LineGeometry>(e).dirty(r);
    }

    void on_destroy_LineDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<LineDetail>(e);
        d = LineDetail();
    }
    void on_destroy_LineStyleDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<LineStyleDetail>(e);
        dispose(d.bind);
        d = LineStyleDetail();
    }
    void on_destroy_LineGeometryDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<LineGeometryDetail>(e);
        dispose(d.node);
        d = LineGeometryDetail();
    }

    void on_update_Line(entt::registry& r, entt::entity e)
    {
        on_destroy_LineDetail(r, e); // reset
        r.get<Line>(e).dirty(r);
    }
    void on_update_LineStyle(entt::registry& r, entt::entity e)
    {
        on_destroy_LineStyleDetail(r, e);
        r.get<LineStyle>(e).dirty(r);
    }
    void on_update_LineGeometry(entt::registry& r, entt::entity e)
    {
        on_destroy_LineGeometryDetail(r, e);
        r.get<LineGeometry>(e).dirty(r);
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

    // Set up our default style detail, which is used when a MeshStyle is missing.
    initializeStyleDetail(getPipelineLayout(Line()), _defaultStyleDetail);
    compile(_defaultStyleDetail.bind);

    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks for Lines
            r.on_construct<Line>().connect<&on_construct_Line>();
            r.on_construct<LineStyle>().connect<&on_construct_LineStyle>();
            r.on_construct<LineGeometry>().connect<&on_construct_LineGeometry>();

            r.on_update<Line>().connect<&on_update_Line>();
            r.on_update<LineStyle>().connect<&on_update_LineStyle>();
            r.on_update<LineGeometry>().connect<&on_update_LineGeometry>();

            r.on_destroy<LineDetail>().connect<&on_destroy_LineDetail>();
            r.on_destroy<LineStyleDetail>().connect<&on_destroy_LineStyleDetail>();
            r.on_destroy<LineGeometryDetail>().connect<&on_destroy_LineGeometryDetail>();

            // Set up the dirty tracking.
            auto e = r.create();
            r.emplace<Line::Dirty>(e);
            r.emplace<LineStyle::Dirty>(e);
            r.emplace<LineGeometry::Dirty>(e);
        });
}

void
LineSystemNode::createOrUpdateComponent(const Line& line, LineDetail& lineDetail, LineGeometryDetail* geomDetail)
{
    // NB: registry is read-locked
    if (geomDetail)
    {
        lineDetail.node = geomDetail->node;
    }
}

void
LineSystemNode::createOrUpdateGeometry(const LineGeometry& geom, LineGeometryDetail& geomDetail, VSGContext& vsgcontext)
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
LineSystemNode::createOrUpdateStyle(const LineStyle& style, LineStyleDetail& styleDetail)
{
    // NB: registry is read-locked
    bool needsCompile = false;
    bool needsUpload = false;

    if (!styleDetail.bind)
    {
        auto layout = getPipelineLayout(Line());
        initializeStyleDetail(layout, styleDetail);
        needsCompile = true;
    }

    // update the uniform for this style:
    auto& uniforms = *static_cast<LineStyleUniform*>(styleDetail.styleData->dataPointer());
    uniforms.style.populate(style);
    needsUpload = !needsCompile;

    if (needsCompile)
        compile(styleDetail.bind);
    else if (needsUpload)
        upload(styleDetail.styleUBO->bufferInfoList);
}

void
LineSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    detail::RenderingState rs {
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    std::vector<LineStyleDetail*> styleDetails;
    styleDetails.emplace_back(&_defaultStyleDetail);

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            for (auto&& [entity, styleDetail] : reg.view<LineStyleDetail>().each())
            {
                styleDetails.emplace_back(&styleDetail);
            }

            int count = 0;
            auto view = reg.view<Line, LineDetail, ActiveState, Visibility>();
            for (auto&& [entity, comp, compDetail, active, visibility] : view.each())
            {
                auto* styleDetail = &_defaultStyleDetail;
                auto* style = reg.try_get<LineStyle>(comp.style);
                if (style)
                {
                    styleDetail = &reg.get<LineStyleDetail>(comp.style);
                }

                if (compDetail.node && visible(visibility, rs))
                {
                    auto* transformDetail = reg.try_get<TransformDetail>(entity);
                    if (transformDetail)
                    {
                        if (transformDetail->passingCull(rs))
                        {
                            styleDetail->drawList.emplace_back(LineDrawable{ compDetail.node, transformDetail });
                            ++count;
                        }
                    }
                    else
                    {
                        styleDetail->drawList.emplace_back(LineDrawable{ compDetail.node, nullptr });
                        ++count;
                    }
                }
            }

            // Render collected data.
            // TODO: swap vectors into unprotected space to free up the readlock?
            if (count > 0)
            {
                _pipelines[0].commands->accept(record);

                for (auto& styleDetail : styleDetails)
                {
                    if (!styleDetail->drawList.empty())
                    {
                        styleDetail->bind->accept(record);

                        for (auto& drawable : styleDetail->drawList)
                        {
                            if (drawable.xformDetail)
                            {
                                drawable.xformDetail->push(record);
                            }

                            drawable.node->accept(record);

                            if (drawable.xformDetail)
                            {
                                drawable.xformDetail->pop(record);
                            }
                        }

                        styleDetail->drawList.clear();
                    }
                }
            }
        });
}

void
LineSystemNode::update(VSGContext& vsgcontext)
{
    if (status.failed()) return;

    // start by disposing of any old static objects
    if (!s_toDispose->children.empty())
    {
        dispose(s_toDispose);
        s_toDispose = vsg::Objects::create();
    }

    _registry.read([&](entt::registry& reg)
        {
            LineStyle::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [style, styleDetail] = reg.get<LineStyle, LineStyleDetail>(e);
                    createOrUpdateStyle(style, styleDetail);
                });

            LineGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [geom, geomDetail] = reg.get<LineGeometry, LineGeometryDetail>(e);
                    createOrUpdateGeometry(geom, geomDetail, vsgcontext);
                });

            Line::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [line, lineDetail] = reg.get<Line, LineDetail>(e);
                    auto* geomDetail = line.geometry != entt::null ? reg.try_get<LineGeometryDetail>(line.geometry) : nullptr;
                    createOrUpdateComponent(line, lineDetail, geomDetail);
                });                    
        });

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
