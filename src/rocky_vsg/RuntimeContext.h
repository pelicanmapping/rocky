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
#include <vsg/app/CompileManager.h>
#include <vsg/app/UpdateOperations.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/SharedObjects.h>

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
        //! Compiler for new vsg objects
        std::function<vsg::CompileManager*()> compiler;

        //! Update operations queue
        std::function<vsg::UpdateOperations*()> updates;

        //! Pool of threads used to load terrain data
        vsg::ref_ptr<vsg::OperationThreads> loaders;

        //! VSG state sharing
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! Function for creating a collection of nodes
        using NodeProvider = std::function<vsg::ref_ptr<vsg::Node>(Cancelable*)>;

        //! Schedules data creation; the resulting node or nodes 
        //! get added to "parent" if the operation suceeds.
        //! Returns a future you can check for completion.
        util::Future<bool> compileAndAddNode(
            vsg::Group* parent,
            NodeProvider provider);

        void removeNode(
            vsg::Group* parent,
            unsigned index);
    };
}
