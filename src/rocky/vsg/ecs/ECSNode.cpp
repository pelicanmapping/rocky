/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ECSNode.h"

#include "MeshSystem.h"
#include "LineSystem.h"
#include "PointSystem.h"
#include "LabelSystem.h"
#include "WidgetSystem.h"
#include "TransformSystem.h"
#include "ModelSystem.h"
#include "NodeGraphSystem.h"
#include "OpticsSystem.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    struct PipelineCompileNode : public vsg::Inherit<vsg::Compilable, PipelineCompileNode>
    {
        std::vector<vsg::ref_ptr<vsg::Commands>> pipelines;

        void compile(vsg::Context& context) override
        {
            for (auto& pipeline : pipelines)
                if (pipeline)
                    pipeline->compile(context);
        }
    };
}


SimpleSystemNodeBase::SimpleSystemNodeBase(Registry& in_registry) :
    System(in_registry)
{
    _toCompile = vsg::Objects::create();
    _toDispose = vsg::Objects::create();

    _tempMT = vsg::MatrixTransform::create();
    _tempMT->children.resize(1);

    // Stub DepthSorted node so the VSG compiler will register its bin for use
    // upon first compile. Not actually used for anything.
    _depthSortedStub = vsg::DepthSorted::create();
    _depthSortedStub->binNumber = 1; // positive bin number ==> DESCENDING sort order (distance)
    _depthSortedStub->child = vsg::Node::create();
}

bool
SimpleSystemNodeBase::firstCompileForView(vsg::Context& context) const
{
    auto view = context.view.ref_ptr();
    if (!view)
        return true;

    return _compiledViews.emplace(view.get()).second;
}

vsg::ref_ptr<vsg::Node>
SimpleSystemNodeBase::pipelineCompileNode() const
{
    auto node = PipelineCompileNode::create();
    node->pipelines.reserve(_pipelines.size());
    for (const auto& pipeline : _pipelines)
        if (pipeline.commands)
            node->pipelines.emplace_back(pipeline.commands);
    return node;
}

ViewIDType
SimpleSystemNodeBase::prepareGeometryView(
    ViewIDType viewID,
    const SRS& worldSRS,
    FrameCountType frame,
    bool persistent) const
{
    auto& view = _viewInfo[viewID];
    view.lastFrame = frame;
    view.persistent = view.persistent || persistent;

    const auto srsDef = worldSRS.definition();
    const auto invalidViewID = std::numeric_limits<ViewIDType>::max();

    bool cacheInvalid =
        view.geometryViewID == invalidViewID ||
        view.geometryViewID >= _viewInfo.size() ||
        _viewInfo[view.geometryViewID].srsDef != srsDef;

    if (view.srsDef != srsDef || cacheInvalid)
    {
        view.srsDef = srsDef;
        view.geometryViewID = viewID;

        // Prefer an already-established lower view ID. Application views are
        // normally registered before render-to-texture views, making the main
        // view the stable owner of shared geometry.
        for (ViewIDType candidate = 0; candidate < viewID; ++candidate)
        {
            const auto& candidateView = _viewInfo[candidate];
            if (candidateView.srsDef == srsDef &&
                candidateView.geometryViewID != invalidViewID &&
                candidateView.geometryViewID < _viewInfo.size() &&
                _viewInfo[candidateView.geometryViewID].srsDef == srsDef)
            {
                view.geometryViewID = candidateView.geometryViewID;
                break;
            }
        }

        // Only an owning view needs its own geometry regenerated.
        view.dirty = view.geometryViewID == viewID;
    }

    return view.geometryViewID;
}

void
SimpleSystemNodeBase::update(VSGContext vsgcontext)
{
    if (!_pipelinesCompiled)
    {
        if (!_pipelines.empty())
            requestCompile(_pipelines[0].commands);
        _pipelinesCompiled = true;
    }

    // compiles:
    if (_toCompile->children.size() > 0)
    {
        auto r = vsgcontext->compile(_toCompile);
        _toCompile->children.clear();

        if (!r)
        {
            Log()->critical("Compile failure in {}. {}", className(), r.message);
            status = Failure(Failure::AssertionFailure, "Compile failure");
        }
    }

    // disposals
    if (_toDispose->children.size() > 0)
    {
        vsgcontext->dispose(_toDispose);
        _toDispose = vsg::Objects::create();
    }

    // uploads:
    if (!_buffersToUpload.empty())
    {
        vsgcontext->upload(_buffersToUpload);
        _buffersToUpload.clear();
    }
    if (!_imagesToUpload.empty())
    {
        vsgcontext->upload(_imagesToUpload);
        _imagesToUpload.clear();
    }

    System::update(vsgcontext);
}

void
SimpleSystemNodeBase::traverse(vsg::Visitor& visitor)
{
    if (status.failed()) return;

    for (auto& pipeline : _pipelines)
    {
        pipeline.commands->accept(visitor);
    }
    Inherit::traverse(visitor);
}

void
SimpleSystemNodeBase::traverse(vsg::ConstVisitor& visitor) const
{
    if (status.failed()) return;

    for (auto& pipeline : _pipelines)
    {
        pipeline.commands->accept(visitor);
    }
    Inherit::traverse(visitor);
}

std::tuple<detail::RenderDomain, entt::entity>
SimpleSystemNodeBase::getRenderDomainAndOverlayTarget(vsg::RecordTraversal& visitor) const
{
    auto request = detail::getRenderRequest(visitor);
    return {request.purpose, request.controller};
}

detail::RenderRequest
SimpleSystemNodeBase::getRenderRequest(vsg::RecordTraversal& visitor) const
{
    return detail::getRenderRequest(visitor);
}


ECSNode::ECSNode(Registry& reg) :
    registry(reg)
{
    // nop
}

ECSNode::ECSNode(Registry& reg, bool addDefaultSystems) :
    ECSNode(reg)
{
    if (addDefaultSystems)
    {
        add(TransformSystemNode::create(registry));
        add(OpticsSystemNode::create(registry));
        add(NodeSystemNode::create(registry));
        add(ModelSystemNode::create(registry));
        add(MeshSystemNode::create(registry));
        add(LineSystemNode::create(registry));
        add(PointSystemNode::create(registry));
        add(LabelSystem::create(registry));
#ifdef ROCKY_HAS_IMGUI
        add(WidgetSystemNode::create(registry));
#endif
    }
}

ECSNode::~ECSNode()
{
    // nop
}

void
ECSNode::initialize(VSGContext vsgcontext)
{
    _vsgcontext = vsgcontext;
    for (auto& system : systems)
    {
        system->initialize(vsgcontext);
    }
}

void
ECSNode::update(VSGContext vsgcontext)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(vsgcontext);
    }
}
