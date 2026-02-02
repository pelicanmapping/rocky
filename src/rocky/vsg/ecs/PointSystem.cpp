/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "PointSystem.h"
#include "TransformDetail.h"
#include "ECSVisitors.h"
#include "../PipelineState.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define VERT_SHADER "shaders/rocky.point.vert"
#define FRAG_SHADER "shaders/rocky.point.frag"

#define LAYOUT_SET 0
#define LAYOUT_BINDING_UNIFORM  1 // layout(set=0, binding=1) in the shader

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(VSGContext& vsgcontext)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(VERT_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(FRAG_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color", "", 1, VK_FORMAT_R32G32B32A32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_width", "", 2, VK_FORMAT_R32_SFLOAT, {});

        shaderSet->addDescriptorBinding("point", "", LAYOUT_SET, LAYOUT_BINDING_UNIFORM,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }


    // creates an empty, default style detail bind command, ready to be populated.
    void initializeStyleDetail(vsg::PipelineLayout* layout, PointStyleDetail& styleDetail)
    {
        // uniform: "point.styles" in the shader
        styleDetail.styleData = vsg::ubyteArray::create(sizeof(PointStyleUniform));
        styleDetail.styleUBO = vsg::DescriptorBuffer::create(styleDetail.styleData, LAYOUT_BINDING_UNIFORM, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // bind command:
        styleDetail.bind = vsg::BindDescriptorSet::create();
        styleDetail.bind->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        styleDetail.bind->firstSet = 0;
        styleDetail.bind->layout = layout;
        styleDetail.bind->descriptorSet = vsg::DescriptorSet::create(
            styleDetail.bind->layout->setLayouts.front(),
            vsg::Descriptors{ styleDetail.styleUBO });

        auto& uniforms = *static_cast<PointStyleUniform*>(styleDetail.styleData->dataPointer());
        uniforms.style = PointStyleRecord(); // default style
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

    void on_construct_Point(entt::registry& r, entt::entity e)
    {
        (void)r.get_or_emplace<ActiveState>(e);
        (void)r.get_or_emplace<Visibility>(e);
        Point::dirty(r, e);
    }
    void on_construct_PointStyle(entt::registry& r, entt::entity e)
    {
        r.emplace<PointStyleDetail>(e);
        PointStyle::dirty(r, e);
    }
    void on_construct_PointGeometry(entt::registry& r, entt::entity e)
    {
        r.emplace<PointGeometryDetail>(e);
        PointGeometry::dirty(r, e);
    }

    void on_destroy_PointStyle(entt::registry& r, entt::entity e)
    {
        r.remove<PointStyleDetail>(e);
    }
    void on_destroy_PointStyleDetail(entt::registry& r, entt::entity e)
    {
        dispose(r.get<PointStyleDetail>(e).bind);
    }
    void on_destroy_PointGeometry(entt::registry& r, entt::entity e)
    {
        r.remove<PointGeometryDetail>(e);
    }
    void on_destroy_PointGeometryDetail(entt::registry& r, entt::entity e)
    {
        dispose(r.get<PointGeometryDetail>(e).rootNode);
    }

    void on_update_Point(entt::registry& r, entt::entity e)
    {
        Point::dirty(r, e);
    }
    void on_update_PointStyle(entt::registry& r, entt::entity e)
    {
        PointStyle::dirty(r, e);
    }
    void on_update_PointGeometry(entt::registry& r, entt::entity e)
    {
        PointGeometry::dirty(r, e);
    }
}

PointSystemNode::PointSystemNode(Registry& registry) :
    Inherit(registry)
{
    // temporary transform used by the visitor traversal(s)
    _tempMT = vsg::MatrixTransform::create();
    _tempMT->children.resize(1);

    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks for Points
            r.on_construct<Point>().connect<&on_construct_Point>();
            r.on_construct<PointStyle>().connect<&on_construct_PointStyle>();
            r.on_construct<PointGeometry>().connect<&on_construct_PointGeometry>();

            r.on_update<Point>().connect<&on_update_Point>();
            r.on_update<PointStyle>().connect<&on_update_PointStyle>();
            r.on_update<PointGeometry>().connect<&on_update_PointGeometry>();

            r.on_destroy<PointStyle>().connect<&on_destroy_PointStyle>();
            r.on_destroy<PointStyleDetail>().connect<&on_destroy_PointStyleDetail>();
            r.on_destroy<PointGeometry>().connect<&on_destroy_PointGeometry>();
            r.on_destroy<PointGeometryDetail>().connect<&on_destroy_PointGeometryDetail>();

            // Set up the dirty tracking.
            auto e = r.create();
            r.emplace<Point::Dirty>(e);
            r.emplace<PointStyle::Dirty>(e);
            r.emplace<PointGeometry::Dirty>(e);
        });
}

void
PointSystemNode::initialize(VSGContext& vsgcontext)
{
    // Now create the pipeline and stategroup to bind it
    auto shaderSet = createShaderSet(vsgcontext);

    if (!shaderSet)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Shaders are missing or corrupt. "
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
        c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        c.config->enableArray("in_width", VK_VERTEX_INPUT_RATE_VERTEX, 4);

        // Uniforms we will need:
        c.config->enableDescriptor("point");

        // always both
        PipelineUtils::enableViewDependentData(c.config);

        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::InputAssemblyState& state) override {
                state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            }
            void apply(vsg::RasterizationState& state) override {
                state.cullMode = VK_CULL_MODE_NONE;
                //state.rasterizerDiscardEnable = VK_TRUE;
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

    // Set up our default style detail, which is used when a style is missing.
    initializeStyleDetail(getPipelineLayout(Point()), _defaultStyleDetail);
    requestCompile(_defaultStyleDetail.bind);
}

void
PointSystemNode::compile(vsg::Context& compileContext)
{
    // called during a compile traversal .. e.g., then adding a new View/RenderGraph.
    _registry.read([&](entt::registry& reg)
        {
            reg.view<PointStyleDetail>().each([&](auto& styleDetail)
                {
                    if (styleDetail.bind)
                        styleDetail.bind->compile(compileContext);
                });

            reg.view<PointGeometryDetail>().each([&](auto& geomDetail)
                {
                    if (geomDetail.geomNode)
                        geomDetail.geomNode->compile(compileContext);
                });
        });

    Inherit::compile(compileContext);
}

void
PointSystemNode::createOrUpdateGeometry(const PointGeometry& geom, PointGeometryDetail& geomDetail, VSGContext& vsgcontext)
{
    // NB: registry is read-locked

    bool reallocate =
        geomDetail.geomNode == nullptr ||
        geomDetail.geomNode->allocatedCapacity < geom.points.capacity();

    if (reallocate)
    {
        if (geomDetail.geomNode)
            vsgcontext->dispose(geomDetail.geomNode);

        geomDetail.geomNode = PointGeometryNode::create();

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

                geomDetail.geomNode->set(copy, geom.colors, geom.widths);
            }
            else
            {
                geomDetail.geomNode->set(geom.points, geom.colors, geom.widths);
            }


            localizer_matrix = vsg::translate(to_vsg(offset));
            auto localizer = vsg::MatrixTransform::create(localizer_matrix);
            localizer->addChild(geomDetail.geomNode);
            root = localizer;
        }
        else
        {
            // no reference point -- push raw geometry
            geomDetail.geomNode->set(geom.points, geom.colors, geom.widths);
            root = geomDetail.geomNode;
        }

        geomDetail.rootNode = root;

        requestCompile(geomDetail.geomNode);
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

            geomDetail.geomNode->set(copy, geom.colors, geom.widths);

            auto mt = util::find<vsg::MatrixTransform>(geomDetail.rootNode);
            mt->matrix = vsg::translate(to_vsg(offset));
            localizer_matrix = mt->matrix;
        }
        else
        {
            // no reference point -- push raw geometry
            geomDetail.geomNode->set(geom.points, geom.colors, geom.widths);
        }

        // upload the changed arrays
        requestUpload(geomDetail.geomNode->arrays);
    }
}

void
PointSystemNode::createOrUpdateStyle(const PointStyle& style, PointStyleDetail& styleDetail)
{
    // NB: registry is read-locked
    bool needsCompile = false;
    bool needsUpload = false;

    if (!styleDetail.bind)
    {
        auto layout = getPipelineLayout(Point());
        initializeStyleDetail(layout, styleDetail);
        needsCompile = true;
    }

    // update the uniform for this style:
    auto& uniforms = *static_cast<PointStyleUniform*>(styleDetail.styleData->dataPointer());
    uniforms.style.populate(style);
    uniforms.style.devicePixelRatio = _devicePixelRatio;
    needsUpload = !needsCompile;

    if (needsCompile)
        requestCompile(styleDetail.bind);
    else if (needsUpload)
        requestUpload(styleDetail.styleUBO->bufferInfoList);
}

void
PointSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    RenderingState rs {
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    std::vector<PointStyleDetail*> styleDetails;
    styleDetails.emplace_back(&_defaultStyleDetail);

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            reg.view<PointStyleDetail>().each([&](auto& styleDetail)
                {
                    styleDetails.emplace_back(&styleDetail);
                });

            int count = 0;
            auto view = reg.view<Point, ActiveState, Visibility>();

            view.each([&](auto entity, auto& point, auto& active, auto& visibility)
                {
                    auto* geom = reg.try_get<PointGeometryDetail>(point.geometry);
                    if (!geom || !geom->rootNode)
                        return;

                    auto* styleDetail = &_defaultStyleDetail;
                    auto* style = reg.try_get<PointStyle>(point.style);
                    if (style)
                    {
                        styleDetail = &reg.get<PointStyleDetail>(point.style);
                    }

                    if (visible(visibility, rs))
                    {
                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            if (transformDetail->views[rs.viewID].passingCull)
                            {
                                styleDetail->drawList.emplace_back(PointDrawable{ geom->rootNode, transformDetail });
                                ++count;
                            }
                        }
                        else
                        {
                            styleDetail->drawList.emplace_back(PointDrawable{ geom->rootNode, nullptr });
                            ++count;
                        }
                    }
                });

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
PointSystemNode::traverse(vsg::ConstVisitor& v) const
{
    for (auto& pipeline : _pipelines)
    {
        pipeline.commands->accept(v);
    }

    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&v);
    std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<Point, ActiveState>();

            view.each([&](auto entity, auto& point, auto& active)
                {
                    auto* geom = reg.try_get<PointGeometryDetail>(point.geometry);
                    if (geom && geom->rootNode)
                    {
                        if (ecsVisitor)
                            ecsVisitor->currentEntity = entity;

                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            _tempMT->matrix = transformDetail->views[viewID].model;
                            _tempMT->children[0] = geom->rootNode;
                            _tempMT->accept(v);
                        }
                        else
                        {
                            geom->rootNode->accept(v);
                        }
                    }
                });
        });

    Inherit::traverse(v);
}

void
PointSystemNode::update(VSGContext& vsgcontext)
{
    if (status.failed()) return;

    // start by disposing of any old static objects
    if (!s_toDispose->children.empty())
    {
        dispose(s_toDispose);
        s_toDispose = vsg::Objects::create();
    }

    if (vsgcontext->devicePixelRatio() != _devicePixelRatio)
    {
        _devicePixelRatio = vsgcontext->devicePixelRatio();

        // If the DPR changed, dirty all styles so the new dpr will get applied
        _registry.read([&](entt::registry& reg)
            {
                for (auto&& [e, style] : reg.view<PointStyle>().each())
                    style.dirty(reg);
            });
    }

    _registry.read([&](entt::registry& reg)
        {
            PointStyle::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [style, styleDetail] = reg.get<PointStyle, PointStyleDetail>(e);
                    createOrUpdateStyle(style, styleDetail);
                });

            PointGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [geom, geomDetail] = reg.get<PointGeometry, PointGeometryDetail>(e);
                    createOrUpdateGeometry(geom, geomDetail, vsgcontext);
                });                   
        });

    Inherit::update(vsgcontext);
}

void
PointGeometryNode::calcBound(vsg::dsphere& output, const vsg::dmat4& matrix) const
{
    int first = firstVertex;
    int count = vertexCount;

    output.reset();

    for(int i = first; i < count; ++i)
    {
        expandBy(output, matrix * vsg::dvec3(_verts->at(i)));
    }
}

void
PointGeometry::recycle(entt::registry& reg)
{
    //auto& geomDetail = reg.get<PointGeometryDetail>(owner);
    //if (geomDetail.geomNode)
    //{
    //    auto geom = util::find<PointGeometryNode>(geomDetail.geomNode);
    //    if (geom)
    //        geom->setCount(0);
    //}
    points.clear();
    dirty(reg);
}
