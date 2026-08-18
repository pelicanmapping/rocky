/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "FutureNode.h"
#include <rocky/weejobs.h>

using namespace ROCKY_NAMESPACE;

FutureNode::FutureNode(Future<NodeResult> f, VSGContext v) :
    _future(f),
    _vsgcontext(v)
{
    //nop
}

void
FutureNode::traverse(vsg::RecordTraversal& record) const
{
    const bool newlyResolved = !_child && resolve();
    if (newlyResolved)
        return;

    if (_child)
        _child->accept(record);
}

bool FutureNode::resolve() const
{
    if (_child)
        return true;
    if (!_future.available())
        return false;

    if (_future->ok())
        _child = _future->value();

    _future = {};

    if (_child && _vsgcontext)
        _vsgcontext->compile(_child);
    if (_vsgcontext)
        _vsgcontext->requestFrame();

    return _child.valid();
}

void
FutureNode::traverse(vsg::Visitor& visitor) 
{
    resolve();
    if (_child)
    {
        _child->accept(visitor);
    }
}

void
FutureNode::traverse(vsg::ConstVisitor& visitor) const 
{
    resolve();
    if (_child)
    {
        _child->accept(visitor);
    }
}
