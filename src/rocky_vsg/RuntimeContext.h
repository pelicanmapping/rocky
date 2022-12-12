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

        template<class T>
        util::Future<bool> runAsyncUpdateSync(
            std::function<Result<T>(Cancelable*)> async,
            std::function<void(const T&, Cancelable*)> sync);
    };


    // inlines

    template<class T>
    util::Future<bool> RuntimeContext::runAsyncUpdateSync(
        std::function<Result<T>(Cancelable*)> async,
        std::function<void(const T&, Cancelable*)> sync)
    {
        struct ComboOp : public vsg::Inherit<vsg::Operation, ComboOp> {
            RuntimeContext* _runtime;
            std::function<Result<T>(rocky::Cancelable*)> _async;
            std::function<void(const T&, Cancelable*)> _sync;
            Result<T> _result;
            util::Promise<bool> _promise;
            Cancelable* _p = nullptr;
            void run() override {
                if (!_promise.isAbandoned()) {
                    if (!_result.status.ok()) {
                        _result = _async(_p);
                        if (_result.status.ok()) {
                            _runtime->updates()->add(vsg::ref_ptr<vsg::Operation>(this));
                            _promise.resolve(true);
                        }
                    }
                    else {
                        _sync(_result.value, _p);
                        _promise.resolve(true);
                    }
                }
            }
        };

        auto op = ComboOp::create();
        op->_runtime = this;
        op->_async = async;
        op->_sync = sync;
        auto result = op->_promise.getFuture();
        this->loaders->add(op);
        return result;
    }

}

