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
    struct AddChildrenSafely : public vsg::Inherit<vsg::Operation, AddChildrenSafely>
    {
        // raw pointer b/c this operation is added directy to Viewer
        vsg::Viewer* _viewer;

        // observer since the parent might disappear
        vsg::observer_ptr<vsg::Group> _parent;

        // ref because childref may only exist here
        std::vector<vsg::ref_ptr<vsg::Node>> _children;

        // promise that must be fulfilled
        util::Promise<bool> _promise;

        AddChildrenSafely(vsg::Viewer* viewer, vsg::Group* parent) :
            _viewer(viewer), _parent(parent)
        {
            //nop
        }

        void run() override
        {
            vsg::ref_ptr<vsg::Group> parent(_parent);
            if (parent)
            {
                // Don't think we actually need this most of the time...
                // revisit...
                //vsg::updateViewer(*_viewer, result);

                for (auto& child : _children)
                {
                    parent->addChild(child);
                }

                _promise.resolve(true);
            }
        }
    };

    struct RunAndAdd : public vsg::Inherit<vsg::Operation, RunAndAdd>
    {
        vsg::Viewer* _viewer;
        vsg::observer_ptr<vsg::Group> _parent;
        RuntimeContext::Creator _func;
        vsg::ref_ptr<AddChildrenSafely> _adder;
           
        RunAndAdd(
            vsg::Viewer* viewer,
            vsg::Group* parent,
            RuntimeContext::Creator func) :

            _viewer(viewer),
            _parent(parent),
            _func(func),
            _adder(AddChildrenSafely::create(viewer, parent))
        {
        }

        void run() override
        {
            if (_func(_adder->_children, nullptr))
            {
                _viewer->updateOperations->add(_adder);
            }
        }
    };

}

RuntimeContext::RuntimeContext()
{
    loaders = vsg::OperationThreads::create(4);
}

//util::Future<bool>
//RuntimeContext::addChildren(
//    vsg::Group* parent,
//    std::vector<vsg::ref_ptr<vsg::Node>> children,
//    util::Promise<bool> promise)
//{
//    vsg::ref_ptr<vsg::Viewer> v = viewer;
//    if (v)
//    {
//        v->addUpdateOperation(
//            AddChildrenSafely::create(v.get(), parent, children, promise),
//            vsg::UpdateOperations::ONE_TIME);
//    }
//}

util::Future<bool>
RuntimeContext::addChildren(
    vsg::Group* parent, Creator func)
{
    vsg::ref_ptr<vsg::Viewer> viewer_safe(viewer);
    if (viewer_safe)
    {
        auto runner = RunAndAdd::create(viewer_safe.get(), parent, func);
        auto future = runner->_adder->_promise.getFuture();
        loaders->add(runner);
        return future;
    }
    else return util::Future<bool>();
}
