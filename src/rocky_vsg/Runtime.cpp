/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Runtime.h"
#include "LineState.h"
#include "Utils.h"
#include <vsg/app/Viewer.h>
#include <shared_mutex>

using namespace ROCKY_NAMESPACE;

namespace
{
    /**
    * An update operation that maintains a priroity queue for update tasks.
    * This sits in the VSG viewer's update operations queue indefinitely
    * and runs once per frame. It chooses the highest priority task in its
    * queue and runs it. It will run one task per frame so that we do not
    * risk frame drops. It will automatically discard any tasks that have
    * been abandoned (no Future exists).
    */
    struct PriorityUpdateQueue : public vsg::Inherit<vsg::Operation, PriorityUpdateQueue>
    {
        std::mutex _mutex;

        struct Task {
            vsg::ref_ptr<vsg::Operation> function;
            std::function<float()> get_priority;
        };
        std::vector<Task> _queue;

        // runs one task per frame.
        void run() override
        {
            if (!_queue.empty())
            {
                Task task;
                {
                    std::scoped_lock lock(_mutex);

                    // sort from low to high priority
                    std::sort(_queue.begin(), _queue.end(),
                        [](const Task& lhs, const Task& rhs)
                        {
                            if (lhs.get_priority == nullptr)
                                return false;
                            else if (rhs.get_priority == nullptr)
                                return true;
                            else
                                return lhs.get_priority() < rhs.get_priority();
                        }
                    );

                    while (!_queue.empty())
                    {
                        // pop the highest priority off the back.
                        task = _queue.back();
                        _queue.pop_back();

                        // check for cancelation - if the task is already canceled, 
                        // discard it and fetch the next one.
                        auto po = dynamic_cast<Cancelable*>(task.function.get());
                        if (po == nullptr || !po->canceled())
                            break;
                        else
                            task = { };
                    }
                }

                if (task.function)
                {
                    task.function->run();
                }
            }
        }
    };

    //! Operation that asynchronously creates a node (via a user lambda function)
    //! and then safely adds it to the `ene graph in the update phase.
    struct AddNodeAsync : public vsg::Inherit<vsg::Operation, AddNodeAsync>
    {
        Runtime _runtime;

        // parent to which to add the child
        vsg::observer_ptr<vsg::Group> _parent;

        // function that will provide the child to add
        Runtime::NodeFactory _childFactory;

        // ref because child probably only exists here
        vsg::ref_ptr<vsg::Node> _child;

        // promise that will be resolved after this operation runs
        util::Future<bool> _promise;
           
        AddNodeAsync(
            const Runtime& runtime,
            vsg::Group* parent,
            Runtime::NodeFactory func) :

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


Runtime::Runtime()
{
    readerWriterOptions = vsg::Options::create();

    shaderCompileSettings = vsg::ShaderCompileSettings::create();

    _priorityUpdateQueue = PriorityUpdateQueue::create();
}

void
Runtime::runDuringUpdate(
    vsg::ref_ptr<vsg::Operation> function,
    std::function<float()> get_priority)
{
    auto pq = dynamic_cast<PriorityUpdateQueue*>(_priorityUpdateQueue.get());
    if (pq)
    {
        std::scoped_lock lock(pq->_mutex);

        if (pq->referenceCount() == 1)
        {
            updates()->add(_priorityUpdateQueue, vsg::UpdateOperations::ALL_FRAMES);
        }

        pq->_queue.push_back({ function, get_priority });
    }
}

util::Future<bool>
Runtime::compileAndAddChild(vsg::ref_ptr<vsg::Group> parent, NodeFactory factory, const util::job& job_config)
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

    util::Future<bool> promise;
    auto& runtime = *this;
    
    auto compiler = runtime.compiler();
    auto updates = runtime.updates();

    auto async_create_and_add_node = [compiler, updates, promise, parent, factory](Cancelable& c) -> bool
    {
        if (c.canceled())
            return false;

        // create the child:
        auto child = factory(c);
        if (!child)
            return false;

        // compile the child:
        compiler->compile(child);

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
        updates->add(promise_op);

        return true;
    };

    auto future = util::job::dispatch(
        async_create_and_add_node,
        promise,
        job_config
    );

    return future;
}

void
Runtime::removeNode(vsg::Group* parent, unsigned index)
{
    auto remover = RemoveNodeAsync::create(parent, index);
    updates()->add(remover);
}

//LineState&
//Runtime::lineState()
//{
//    if (!_LineState)
//    {
//        _LineState = std::make_shared<LineState>(*this);
//    }
//    return *_LineState.get();
//}

void
Runtime::dirty(vsg::Object* object)
{
    // TODO.
    // For now, this will immediately recompile and object.
    // We may want to instead queue it up and do it asynchronously for 
    // objects that are already in the scene graph and are dynamic.
    ROCKY_SOFT_ASSERT_AND_RETURN(object, void());
    // crash
    //compiler()->compile(vsg::ref_ptr<vsg::Object>(object));
}
