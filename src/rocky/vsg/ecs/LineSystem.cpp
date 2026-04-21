/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "LineSystem.h"
#include "ECSVisitors.h"
#include "TransformDetail.h"
#include "../PipelineState.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define LINE_VERT_SHADER "shaders/rocky.line.vert"
#define LINE_FRAG_SHADER "shaders/rocky.line.frag"

#define LINE_SET 0
#define LINE_BINDING_UNIFORM  1 // layout(set=0, binding=1) in the shader

#define USE_DYNAMIC_STATE

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createLineShaderSet(VSGContext vsgcontext)
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
        shaderSet->addAttributeBinding("in_vertex",      "", 0, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_vertex_prev", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_vertex_next", "", 2, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color",       "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, {});

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
        (void) r.get_or_emplace<ActiveState>(e);
        (void) r.get_or_emplace<Visibility>(e);
        Line::dirty(r, e);
    }
    void on_construct_LineStyle(entt::registry& r, entt::entity e)
    {
        r.emplace<LineStyleDetail>(e);
        LineStyle::dirty(r, e);
    }
    void on_construct_LineGeometry(entt::registry& r, entt::entity e)
    {
        r.emplace<LineGeometryDetail>(e);
        LineGeometry::dirty(r, e);
    }

    void on_destroy_LineStyle(entt::registry& r, entt::entity e)
    {
        r.remove<LineStyleDetail>(e);
    }
    void on_destroy_LineStyleDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<LineStyleDetail>(e);
        dispose(d.bind);
        for (auto& pass : d.passes)
            dispose(pass);
    }
    void on_destroy_LineGeometry(entt::registry& r, entt::entity e)
    {
        r.remove<LineGeometryDetail>(e);
    }
    void on_destroy_LineGeometryDetail(entt::registry& r, entt::entity e)
    {
        for (auto& view : r.get<LineGeometryDetail>(e).views)
        {
            dispose(view.root);
            view = {};
        }
    }

    void on_update_Line(entt::registry& r, entt::entity e)
    {
        Line::dirty(r, e);
    }
    void on_update_LineStyle(entt::registry& r, entt::entity e)
    {
        LineStyle::dirty(r, e);
    }
    void on_update_LineGeometry(entt::registry& r, entt::entity e)
    {
        LineGeometry::dirty(r, e);
    }
}

LineSystemNode::LineSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks for Lines
            r.on_construct<Line>().connect<&on_construct_Line>();
            r.on_construct<LineStyle>().connect<&on_construct_LineStyle>();
            r.on_construct<LineGeometry>().connect<&on_construct_LineGeometry>();

            r.on_update<Line>().connect<&on_update_Line>();
            r.on_update<LineStyle>().connect<&on_update_LineStyle>();
            r.on_update<LineGeometry>().connect<&on_update_LineGeometry>();

            r.on_destroy<LineStyle>().connect<&on_destroy_LineStyle>();
            r.on_destroy<LineStyleDetail>().connect<&on_destroy_LineStyleDetail>();
            r.on_destroy<LineGeometry>().connect<&on_destroy_LineGeometry>();
            r.on_destroy<LineGeometryDetail>().connect<&on_destroy_LineGeometryDetail>();

            // Set up the dirty tracking.
            auto e = r.create();
            r.emplace<Line::Dirty>(e);
            r.emplace<LineStyle::Dirty>(e);
            r.emplace<LineGeometry::Dirty>(e);
        });
}

void
LineSystemNode::initialize(VSGContext vsgcontext)
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
                state.depthTestEnable = VK_TRUE;
                //if ((feature_mask & WRITE_DEPTH) == 0) {
                //    state.depthWriteEnable = (feature_mask & WRITE_DEPTH) ? VK_TRUE : VK_FALSE;
                //}
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
    requestCompile(_defaultStyleDetail.bind);
}

void
LineSystemNode::compile(vsg::Context& compileContext)
{
    // called during a compile traversal .. e.g., then adding a new View/RenderGraph.
    _registry.read([&](entt::registry& reg)
        {
            reg.view<LineStyleDetail>().each([&](auto& styleDetail)
                {
                    if (styleDetail.bind)
                        styleDetail.bind->compile(compileContext);
                });

            reg.view<LineGeometryDetail>().each([&](auto& geomDetail)
                {
                    for (auto& geomView : geomDetail.views)
                    {
                        if (geomView.geomNode)
                            geomView.geomNode->compile(compileContext);
                    }
                });
        });

    Inherit::compile(compileContext);
}

void
LineSystemNode::createOrUpdateGeometryForView(ViewIDType viewID, const LineGeometry& geom, LineGeometryDetail& geomDetail)
{
    // NB: registry is read-locked

    auto& geomView = geomDetail.views[viewID];
    auto& view = _viewInfo[viewID];

    // If the view is removed, dispose of its nodes:
    if (view.srsDef.empty())
    {
        if (geomView.root)
        {
            dispose(geomView.root);
            dispose(geomView.geomNode);
            geomView = {};
        }
        view.dirty = false;
        return;
    }

    SRS srs(view.srsDef);
    ROCKY_SOFT_ASSERT_AND_RETURN(srs, void());

    bool reallocate = view.dirty;
    view.dirty = false;

    if (!geomView.root)
    {
        reallocate = true;
    }
    else
    {
        if (geomView.geomNode && geom.points.capacity() > geomView.geomNode->allocatedCapacity)
        {
            reallocate = true;
        }
    }

    if (reallocate)
    {
        // discard the old node and create a new one.
        if (geomView.geomNode)
            dispose(geomView.geomNode);

        geomView.geomNode = LineGeometryNode::create();

        vsg::ref_ptr<vsg::Node> root;
        vsg::dmat4 localizer_matrix;

        if (geom.srs.valid())
        {
            glm::dvec3 precisionOffset(0, 0, 0);
            auto xform = geom.srs.to(srs);

            // make a copy that we will use to transform and offset:
            if (!geom.points.empty())
            {
                std::vector<glm::dvec3> copy(geom.points);
                xform.clampArray(copy.data(), copy.size());
                xform.transformArray(copy.data(), copy.size());

                precisionOffset = copy.front(); // (copy.front() + copy.back()) * 0.5;
                for (auto& point : copy)
                    point -= precisionOffset;

                geomView.geomNode->set(copy, geom.colors, geom.topology);
            }
            else
            {
                geomView.geomNode->set(geom.points, geom.colors, geom.topology);
            }

            localizer_matrix = vsg::translate(to_vsg(precisionOffset));
            auto localizer = vsg::MatrixTransform::create(localizer_matrix);
            localizer->addChild(geomView.geomNode);
            root = localizer;
        }
        else
        {
            // no reference point -- push raw geometry
            geomView.geomNode->set(geom.points, geom.colors, geom.topology);
            root = geomView.geomNode;
        }

        geomView.root = root;

        requestCompile(geomView.root);
    }

    else // existing node -- update:
    {
        vsg::dsphere bound;
        vsg::dmat4 localizer_matrix;

        if (geom.srs.valid() && geom.points.size() > 0)
        {
            glm::dvec3 precisionOffset(0, 0, 0);
            auto xform = geom.srs.to(srs);

            // make a copy that we will use to transform and offset:
            std::vector<glm::dvec3> copy(geom.points);
            xform.clampArray(copy.data(), copy.size());
            xform.transformArray(copy.data(), copy.size());

            precisionOffset = copy.front(); // (copy.front() + copy.back()) * 0.5;
            for (auto& point : copy)
                point -= precisionOffset;

            geomView.geomNode->set(copy, geom.colors, geom.topology);

            auto mt = find<vsg::MatrixTransform>(geomView.root);
            mt->matrix = vsg::translate(to_vsg(precisionOffset));
            localizer_matrix = mt->matrix;
        }
        else
        {
            // no reference point -- push raw geometry
            geomView.geomNode->set(geom.points, geom.colors, geom.topology);
        }

        // upload the changed arrays
        requestUpload(geomView.geomNode->arrays);
        requestUpload(geomView.geomNode->indices);
    }
}

void
LineSystemNode::createOrUpdateGeometry(const LineGeometry& geom, LineGeometryDetail& geomDetail)
{
    // NB: registry is read-locked
    for (ViewIDType viewID = 0; viewID < _viewInfo.size(); ++viewID)
    {
        createOrUpdateGeometryForView(viewID, geom, geomDetail);
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

    for (auto& pass : styleDetail.passes)
        dispose(pass);

#ifdef USE_DYNAMIC_STATE
    styleDetail.passes.clear();
    styleDetail.passes.emplace_back(vsg::Commands::create());
    styleDetail.passes[0]->addChild(styleDetail.bind);
    // NOP - Empty pass for now.
#endif

    // update the uniform for this style:
    auto& uniforms = *static_cast<LineStyleUniform*>(styleDetail.styleData->dataPointer());
    uniforms.style.populate(style);
    uniforms.style.devicePixelRatio = _devicePixelRatio;
    needsUpload = !needsCompile;

    if (needsCompile)
        requestCompile(styleDetail.bind);
    else if (needsUpload)
        requestUpload(styleDetail.styleUBO->bufferInfoList);
}

void
LineSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    auto vp = record.getCommandBuffer()->viewDependentState->view->camera->getViewport();
    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount,
        { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
    };

    _styleDetailBins.clear();
    _styleDetailBins.emplace_back(&_defaultStyleDetail);

    SRS srs;
    record.getValue("rocky.worldsrs", srs);

    auto& view = _viewInfo[rs.viewID];

    // I'm alive
    view.lastFrame = rs.frame;

    // Did my SRS change? Because if it did, we need to regenerate
    if (view.srsDef.empty() || view.srsDef != srs.definition())
    {
        view.srsDef = srs.definition();
        view.dirty = true;
        return;
    }

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            reg.view<LineStyle, LineStyleDetail>().each([&](auto& style, auto& styleDetail)
                {
                    _styleDetailBins.emplace_back(&styleDetail);
                    styleDetail.useTransparencyBin = style.transparencyBin;
                });

            int count = 0;

            auto iter = reg.view<Line, ActiveState, Visibility>();
            iter.each([&](auto entity, auto& line, auto& active, auto& visibility)
                {
                    auto* geomDetail = reg.try_get<LineGeometryDetail>(line.geometry);
                    if (!geomDetail)
                        return;

                    auto& geomView = geomDetail->views[rs.viewID];

                    if (geomView.root && visible(visibility, rs))
                    {
                        auto* styleDetail = reg.try_get<LineStyleDetail>(line.style);
                        if (!styleDetail)
                            styleDetail = &_defaultStyleDetail;

                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            if (transformDetail->views[rs.viewID].passingCull)
                            {
                                styleDetail->drawList.emplace_back(geomView.root, transformDetail);
                                ++count;
                            }
                        }
                        else
                        {
                            styleDetail->drawList.emplace_back(geomView.root, nullptr);
                            ++count;
                        }
                    }
                });

            // Render collected data.
            if (count > 0)
            {
                for (auto& styleDetail : _styleDetailBins)
                {
                    if (!styleDetail->drawList.empty())
                    {
                        if (!styleDetail->renderer)
                        {
                            styleDetail->renderer = StyleRenderer<LineStyleDetail>::create();
                            styleDetail->renderer->styleDetail = styleDetail;
                            styleDetail->renderer->pipeline = _pipelines[0].commands;
                        }

                        styleDetail->renderer->accept(record);
                    }
                }
            }
        });
}

void
LineSystemNode::traverse(vsg::ConstVisitor& v) const
{
    handleConstVisitor<Line, LineGeometryDetail>(v);
    Inherit::traverse(v);
}

void
LineSystemNode::traverse(vsg::Visitor& v)
{
    handleVisitor<LineGeometryDetail>(v);
    Inherit::traverse(v);
}

void
LineSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    // start by disposing of any old static objects
    if (!s_toDispose->children.empty())
    {
        dispose(s_toDispose);
        s_toDispose = vsg::Objects::create();
    }

    _registry.read([&](entt::registry& reg)
        {
            // check whether the device pixel ratio has changed; if so we need to redo the style
            if (vsgcontext->devicePixelRatio() != _devicePixelRatio)
            {
                _devicePixelRatio = vsgcontext->devicePixelRatio();

                // If the DPR changed, dirty all styles so the new dpr will get applied
                for (auto&& [e, style] : reg.view<LineStyle>().each())
                    style.dirty(reg);
            }

            // check for dirty styles
            LineStyle::eachDirty(reg, [&](entt::entity e)
                {
                    const auto [style, styleDetail] = reg.try_get<LineStyle, LineStyleDetail>(e);
                    if (style && styleDetail)
                        createOrUpdateStyle(*style, *styleDetail);
                });

            // check for dirty geometry:
            LineGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    const auto [geom, geomDetail] = reg.try_get<LineGeometry, LineGeometryDetail>(e);
                    if (geom && geomDetail)
                        createOrUpdateGeometry(*geom, *geomDetail);
                });

            // Regenerate all geometry for any view that has changed (e.g., an SRS switch or adding a new view)
            for (ViewIDType viewID = 0; viewID < _viewInfo.size(); ++viewID)
            {
                auto& view = _viewInfo[viewID];

                // Check for view expiration (no record traversal)
                auto frame = vsgcontext->viewer()->getFrameStamp()->frameCount;
                if (view.lastFrame < frame - 1u)
                {
                    view.srsDef.clear();
                    view.dirty = true;
                    view.lastFrame = std::numeric_limits<FrameCountType>::max();
                }

                if (view.dirty)
                {
                    reg.view<LineGeometry, LineGeometryDetail>().each([&](auto e, auto& geom, auto& geomDetail)
                        {
                            createOrUpdateGeometryForView(viewID, geom, geomDetail);
                        });
                }
            }
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
    for (auto& geomView : geomDetail.views)
    {
        if (geomView.geomNode)
        {
            geomView.geomNode->setCount(0);
        }
    }
    points.clear();
    dirty(reg);
}
