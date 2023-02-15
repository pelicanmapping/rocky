/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "RuntimeContext.h"
#include "Utils.h"
#include <vsg/app/Viewer.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Operation that asynchronously creates a node (via a user lambda function)
    //! and then safely adds it to the `ene graph in the update phase.
    struct AddNodeAsync : public vsg::Inherit<vsg::Operation, AddNodeAsync>
    {
        RuntimeContext _runtime;

        // parent to which to add the child
        vsg::observer_ptr<vsg::Group> _parent;

        // function that will provide the child to add
        RuntimeContext::NodeFactory _childFactory;

        // ref because child probably only exists here
        vsg::ref_ptr<vsg::Node> _child;

        // promise that will be resolved after this operation runs
        util::Promise<bool> _promise;
           
        AddNodeAsync(
            const RuntimeContext& runtime,
            vsg::Group* parent,
            RuntimeContext::NodeFactory func) :

            _runtime(runtime), _parent(parent), _childFactory(func)
        {
            //nop
        }

        void run() override
        {
            bool error = true;

            if (_promise.canceled())
                return;

            if (!_child)
            {
                // resolve the observer pointers:
                auto compiler = _runtime.compiler();
                auto updates = _runtime.updates();

                if (compiler && updates)
                {
                    // generate the child node:
                    _child = _childFactory(_promise);
                    if (_child)
                    {
                        compiler->compile(_child);

                        // re-queue this operation on the viewer's update queue
                        updates->add(vsg::ref_ptr<vsg::Operation>(this));
                        error = false;
                    }
                }
            }
            else
            {
                vsg::ref_ptr<vsg::Group> parent = _parent;
                if (parent)
                {
                    parent->addChild(_child);
                    _promise.resolve(true);
                    error = false;
                }
            }

            if (error)
            {
                _promise.resolve(false);
            }
        }
    };

    //! Operation that removes a node from the scene graph.
    struct RemoveNodeAsync : public vsg::Inherit<vsg::Operation, RemoveNodeAsync>
    {
        vsg::observer_ptr<vsg::Group> _parent;
        unsigned _index;

        RemoveNodeAsync(vsg::Group* parent, unsigned index) :
            _parent(parent), _index(index) { }

        void run() override
        {
            vsg::ref_ptr<vsg::Group> parent = _parent;
            if (parent && parent->children.size() >= _index)
            {
                auto iter = parent->children.begin() + _index;
                parent->children.erase(iter);
            }
        }
    };
}

RuntimeContext::RuntimeContext()
{
}

util::Future<bool>
RuntimeContext::compileAndAddChild(vsg::Group* parent, NodeFactory factory, const util::job& job_config)
{
    // This is a two-step procedure. First we have to create the child
    // by calling the Factory function, and compile it. These things happen 
    // in the asynchronous function. Secondly, we have to add the node to the
    // scene graph; this happens in VSG's update operations queue in some future
    // frame.

    // In order to return a future to the entire process, we will make our own
    // Promise and pass it along to both the async part and then on to the sync
    // update part. That way the user will be waiting on the final result of the
    // scene graph merge.

    // TODO
    // WARNING. These jobs cannot cancel! Because the Promise object is held in
    // both the async_create_and_add_node lambda, AND in the job delegate lambda,
    // there are two refs to the shared future container and this prevents 
    // cancelation. Need to fix that.

    util::Promise<bool> promise;
    auto& runtime = *this;
    
    auto async_create_and_add_node = [runtime, promise, parent, factory](Cancelable& c) -> bool
    {
        if (c.canceled())
            return false;

        // create the child:
        auto child = factory(c);
        if (!child)
            return false;

        // compile the child:
        runtime.compiler()->compile(child);

        // queue an update operation to add the child safely.
        // we pass along the original promise so these two operations appear as
        // one to the caller.
        auto add_child = [parent, child](Cancelable& c) -> bool
        {
            if (c.canceled())
                return false;

            if (parent && child)
                parent->addChild(child);

            return true;
        };        
        auto promise_op = util::PromiseOperation<bool>::create(promise, add_child);
        runtime.updates()->add(promise_op);

        return true;
    };

    auto future = util::job::dispatch<bool>(
        async_create_and_add_node,
        promise,
        job_config
    );

    return future;
}

void
RuntimeContext::removeNode(vsg::Group* parent, unsigned index)
{
    auto remover = RemoveNodeAsync::create(parent, index);
    updates()->add(remover);
}
