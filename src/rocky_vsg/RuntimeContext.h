/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Instance.h>
#include <rocky/IOTypes.h>
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

namespace ROCKY_NAMESPACE
{
    /**
     * Interface to runtime operations like the VSG compiler, thread pools,
     * and asynchronous scene graph functions.
     */
    class ROCKY_VSG_EXPORT RuntimeContext
    {
    public:
        //! Constructor
        RuntimeContext();

        //! Compiler for new vsg objects
        std::function<vsg::ref_ptr<vsg::CompileManager>()> compiler;

        //! Queue for VSG synchronous update operations. Operations in this queue
        //! can safely edit the scene graph.
        std::function<vsg::ref_ptr<vsg::UpdateOperations>()> updates;

        //! VSG state sharing
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! Queue a function to run during the update pass
        void runDuringUpdate(
            vsg::ref_ptr<vsg::Operation> function,
            std::function<float()> get_priority = nullptr);

        //! Function that creates a node
        using NodeFactory = std::function<vsg::ref_ptr<vsg::Node>(Cancelable&)>;

        //! Schedules data creation; the resulting node or nodes 
        //! get added to "parent" if the operation suceeds.
        //! Returns a future you can check for completion.
        util::Future<bool> compileAndAddChild(
            vsg::ref_ptr<vsg::Group> parent,
            NodeFactory factory,
            const util::job& config = { });

        //! Safely removes a node from the scene graph (async)
        void removeNode(
            vsg::Group* parent,
            unsigned index);

    private:
        vsg::ref_ptr<vsg::Operation> _priorityUpdateQueue;
    };
}

