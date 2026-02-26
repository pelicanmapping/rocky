/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "NodeGraphSystem.h"
#include "ECSVisitors.h"
#include "TransformDetail.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;


NodeSystemNode::NodeSystemNode(Registry& registry) :
    Inherit(registry)
{
    // temporary transform used by the visitor traversal(s)
    _tempMT = vsg::MatrixTransform::create();
    _tempMT->children.resize(1);

    registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<NodeGraph>().connect<&NodeSystemNode::on_construct_NodeGraph>(*this);
            r.on_update<NodeGraph>().connect<&NodeSystemNode::on_update_NodeGraph>(*this);

            auto e = r.create();
            r.emplace<NodeGraph::Dirty>(e);
        });
}

NodeSystemNode::~NodeSystemNode()
{
    _registry.write([&](entt::registry& r)
        {
            r.on_construct<NodeGraph>().disconnect<&NodeSystemNode::on_construct_NodeGraph>(*this);
            r.on_update<NodeGraph>().disconnect<&NodeSystemNode::on_update_NodeGraph>(*this);
        });
}

void
NodeSystemNode::on_construct_NodeGraph(entt::registry& r, entt::entity e)
{
    // TODO: put this in a utility function somewhere
    // common components that may already exist on this entity:
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    NodeGraph::dirty(r, e);
}

void
NodeSystemNode::on_update_NodeGraph(entt::registry& r, entt::entity e)
{
    NodeGraph::dirty(r, e);
}

void
NodeSystemNode::initialize(VSGContext vsgcontext)
{
    // nop
}

void
NodeSystemNode::compile(vsg::Context& compileContext)
{
    if (status.failed()) return;

    // called during a compile traversal .. e.g., then adding a new View/RenderGraph.
    _registry.read([&](entt::registry& reg)
        {
            reg.view<NodeGraph>().each([&](auto& component)
                {
                    // nop
                });
        });

    Inherit::compile(compileContext);
}


void
NodeSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    RenderingState rs
    {
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<NodeGraph, ActiveState, Visibility>();

            view.each([&](auto entity, auto& comp, auto& active, auto& visibility)
                {
                    if (comp.node && visible(visibility, rs))
                    {
                        auto* xformDetail = reg.try_get<TransformDetail>(entity);
                        if (xformDetail)
                        {
                            if (xformDetail->views[rs.viewID].passingCull)
                            {
                                _drawList.emplace_back(comp.node, xformDetail);
                            }
                        }
                        else
                        {
                            _drawList.emplace_back(comp.node, nullptr);
                        }
                    }
                });

            // Render collected data.
            for (auto& drawable : _drawList)
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

            _drawList.clear();
        });
}

void
NodeSystemNode::traverse(vsg::ConstVisitor& v) const
{
    if (status.failed()) return;

    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&v);
    std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<NodeGraph, ActiveState>();

            view.each([&](auto entity, auto& comp, auto& active)
                {
                    if (comp.node)
                    {
                        if (ecsVisitor)
                            ecsVisitor->currentEntity = entity;

                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            _tempMT->matrix = transformDetail->views[viewID].model;
                            _tempMT->children[0] = comp.node;
                            _tempMT->accept(v);
                        }
                        else
                        {
                            comp.node->accept(v);
                        }
                    }
                });
        });

    Inherit::traverse(v);
}

void
NodeSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

#if 0
    // start by disposing of any old static objects
    if (!s_toDispose->children.empty())
    {
        dispose(s_toDispose);
        s_toDispose = vsg::Objects::create();
    }
#endif

    // process any objects marked dirty
    _registry.read([&](entt::registry& reg)
        {
            NodeGraph::eachDirty(reg, [&](entt::entity e)
                {
                    //nop
                });
        });

    Inherit::update(vsgcontext);
}
