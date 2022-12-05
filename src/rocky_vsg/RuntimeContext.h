/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Instance.h>
#include <rocky/Threading.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/threading/OperationThreads.h>

namespace vsg
{
    class Viewer;
    class Group;
    class Node;
}

namespace rocky
{
    /**
     * Access layer to viewer, rendering, scene graph, and 
     * other runtime operations.
     */
    class ROCKY_VSG_EXPORT RuntimeContext
    {
    public:
        RuntimeContext();

        vsg::observer_ptr<vsg::Viewer> viewer;

        vsg::ref_ptr<vsg::OperationThreads> loaders;

        //! Schedules one or more nodes to be added to the
        //! parent group during an update cycle.
        //util::Future<bool> addChildren(
        //    vsg::Group* parent,
        //    std::vector<vsg::ref_ptr<vsg::Node>> nodes,
        //    util::Promise<bool> );

        using Creator = std::function<bool(
            std::vector<vsg::ref_ptr<vsg::Node>>&,
            Cancelable*)>;

        //! Schedules data creation; the resulting node or nodes 
        //! get added to "parent" if the operation suceeds.
        //! Returns a future you can check for completion.
        util::Future<bool> addChildren(
            vsg::Group* parent, Creator);
    };
}
