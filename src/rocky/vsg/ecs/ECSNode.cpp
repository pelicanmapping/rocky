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
using namespace ROCKY_NAMESPACE::detail;


SimpleSystemNodeBase::SimpleSystemNodeBase(Registry& in_registry) :
    System(in_registry)
{
    _toCompile = vsg::Objects::create();
    _toDispose = vsg::Objects::create();
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
ECSNode::initialize(VSGContext vsgcontext)
{
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
