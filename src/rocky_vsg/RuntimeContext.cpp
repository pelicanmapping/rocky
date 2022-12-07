/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "RuntimeContext.h"
#include <vsg/app/Viewer.h>

using namespace rocky;

namespace
{
    struct AddNodeAsync : public vsg::Inherit<vsg::Operation, AddNodeAsync>
    {
        RuntimeContext _runtime;

        // parent to which to add the child
        vsg::observer_ptr<vsg::Group> _parent;

        // function that will provide the child to add
        RuntimeContext::NodeProvider _childProvider;

        // ref because child probably only exists here
        vsg::ref_ptr<vsg::Node> _child;

        // promise that will be resolved after this operation runs
        util::Promise<bool> _promise;
           
        AddNodeAsync(
            const RuntimeContext& runtime,
            //vsg::observer_ptr<vsg::UpdateOperations> updates,
            vsg::Group* parent,
            RuntimeContext::NodeProvider func) :

            _runtime(runtime), _parent(parent), _childProvider(func)
        {
            //nop
        }

        void run() override
        {
            bool error = true;

            if (!_child)
            {
                // resolve the observer pointers:
                auto compiler = _runtime.getCompiler();
                auto updates = _runtime.getUpdates();

                if (compiler && updates)
                {
                    // generate the child node:
                    _child = _childProvider(nullptr);
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

util::Future<bool>
RuntimeContext::compileAndAddNode(vsg::Group* parent, NodeProvider func)
{
    auto runner = AddNodeAsync::create(*this, parent, func);
    auto future = runner->_promise.getFuture();
    loaders->add(runner);
    return future;
}

void
RuntimeContext::removeNode(vsg::Group* parent, unsigned index)
{
    auto remover = RemoveNodeAsync::create(parent, index);
    loaders->add(remover);
}
