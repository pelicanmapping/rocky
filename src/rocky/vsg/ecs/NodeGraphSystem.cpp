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
NodeSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    auto vp = record.getCommandBuffer()->viewDependentState->view->camera->getViewport();
    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount,
        { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
    };

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            auto iter = reg.view<NodeGraph, ActiveState, Visibility>();

            iter.each([&](auto entity, auto& ng, auto& active, auto& visibility)
                {
                    if (ng.node && visible(visibility, rs))
                    {
                        auto* xformDetail = reg.try_get<TransformDetail>(entity);
                        if (xformDetail)
                        {
                            if (xformDetail->views[rs.viewID].passingCull)
                            {
                                _drawList.emplace_back(ng.node, xformDetail);
                            }
                        }
                        else
                        {
                            _drawList.emplace_back(ng.node, nullptr);
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
NodeSystemNode::traverse(vsg::Visitor& visitor)
{
    // Supports the CompileTraversal, for one, which needs to compile the node
    // for any new View that appears
    _registry.read([&](entt::registry& reg)
        {
            reg.view<NodeGraph>().each([&](auto& ng)
                {
                    if (ng.node)
                    {
                        ng.node->accept(visitor);
                    }
                });
        });

    Inherit::traverse(visitor);
}

void
NodeSystemNode::traverse(vsg::ConstVisitor& visitor) const
{
    if (status.failed()) return;

    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&visitor);
    std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

    _registry.read([&](entt::registry& reg)
        {
            auto iter = reg.view<NodeGraph, ActiveState>();

            iter.each([&](auto entity, auto& comp, auto& active)
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
                            _tempMT->accept(visitor);
                        }
                        else
                        {
                            comp.node->accept(visitor);
                        }
                    }
                });
        });

    Inherit::traverse(visitor);
}

void
NodeSystemNode::compile(vsg::Context& cc)
{
    //_registry.read([&](entt::registry& reg)
    //    {
    //        reg.view<NodeGraph>().each([&](auto& component)
    //            {
    //            });
    //    });
    Inherit::compile(cc);
}

void
NodeSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

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
