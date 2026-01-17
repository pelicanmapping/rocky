/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ECSNode.h"

#include "MeshSystem.h"
#include "LineSystem.h"
#include "PointSystem.h"
//#include "IconSystem.h"
#include "LabelSystem.h"
#include "WidgetSystem.h"
#include "TransformSystem.h"
#include "NodeGraphSystem.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;


ECSNode::ECSNode(Registry& reg) :
    registry(reg)
{
    //factory.start();
}

ECSNode::ECSNode(Registry& reg, bool addDefaultSystems) :
    ECSNode(reg)
{
    if (addDefaultSystems)
    {
        add(TransformSystem::create(registry));
        add(NodeSystemNode::create(registry));
        add(MeshSystemNode::create(registry));
        add(LineSystemNode::create(registry));
        add(PointSystemNode::create(registry));
        //add(IconSystemNode::create(registry));
        add(LabelSystem::create(registry));
#ifdef ROCKY_HAS_IMGUI
        add(WidgetSystemNode::create(registry));
#endif
    }
}

ECSNode::~ECSNode()
{
    //factory.quit();
}

void
ECSNode::initialize(VSGContext& vsgcontext)
{
    for (auto& system : systems)
    {
        system->initialize(vsgcontext);
    }
}

void
ECSNode::update(VSGContext& vsgcontext)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(vsgcontext);
    }
}
