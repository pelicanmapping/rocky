/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/Instance.h>
#include <rocky/IOTypes.h>
#include <rocky/Threading.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/app/Viewer.h>
#include <vsg/app/CompileManager.h>
#include <vsg/app/UpdateOperations.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/SharedObjects.h>
#include <vsg/text/Font.h>
#include <shared_mutex>
#include <queue>

namespace vsg
{
    class Viewer;
    class Group;
    class Node;
    class Font;
}

namespace ROCKY_NAMESPACE
{
    /**
     * Properties and operations needed for interfacing with VSG.
     */
    class ROCKY_EXPORT Runtime
    {
    public:
        //! Constructor
        Runtime();

        //! Viewer instance
        vsg::ref_ptr<vsg::Viewer> viewer;

        //! VSG object sharing
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! VSG readerwriter options
        vsg::ref_ptr<vsg::Options> readerWriterOptions;

        //! Search paths for vsg::findFile
        vsg::Paths searchPaths;

        //! Default font
        vsg::ref_ptr<vsg::Font> defaultFont;

        //! Render on demand mode
        bool renderOnDemand = false;

        //! Number of render-on-demand requests
        std::atomic_int renderRequests = { 0 };

        //! Request a frame render
        void requestFrame() {
            ++renderRequests;
        }

        //! Shared shader compile settings. Use this to insert shader defines
        //! that should be used throughout the application; things like enabling
        //! lighting, debug visuals, etc.
        vsg::ref_ptr<vsg::ShaderCompileSettings> shaderCompileSettings;

        //! Revision number associated with the compile settings. A clients can
        //! poll this to see if it needs to regenerate its pipeline.
        Revision shaderSettingsRevision = 0;

        //! If true, compile() will operate immediately regardless
        //! of the calling thread. If false, compilation is deferred
        //! until the next call to update().
        bool asyncCompile = true;

        //! Custom vsg object disposer (optional)
        //! By default Runtime uses its own round-robin object disposer
        std::function<void(vsg::ref_ptr<vsg::Object>)> disposer;

    public:

        //! Queue a function to run during the update pass
        //! This is a safe way to do things that require modifying the scene
        //! or compiling vulkan objects
        void onNextUpdate(
            vsg::ref_ptr<vsg::Operation> function,
            std::function<float()> get_priority = {});

        //! Queue a function to run during the update pass.
        //! This is a safe way to do things that require modifying the scene
        //! or compiling vulkan objects
        void onNextUpdate(
            std::function<void()> function);

        //! Compiles an object now.
        //! Be careful to only call this from a safe thread
        void compile(vsg::ref_ptr<vsg::Object> object);

        //! Destroys a VSG object, eventually. 
        //! Call this to get rid of descriptor sets you plan to replace.
        //! You can't just let them go since they recycle internally and 
        //! you could end up trying to destroy a vulkan object while
        //! compiling new objects, which will result in a validation error
        //! and leaked memory.
        //! https://github.com/vsg-dev/VulkanSceneGraph/discussions/949
        void dispose(vsg::ref_ptr<vsg::Object> object);

        //! Schedules data creation; the resulting node or nodes 
        //! get added to "parent" if the operation suceeds.
        //! Returns a future so you can check for completion.
        jobs::future<bool> compileAndAddChild(
            vsg::ref_ptr<vsg::Group> parent,
            std::function<vsg::ref_ptr<vsg::Node>(Cancelable&)> factory,
            const jobs::context& config = { });

        //! Safely removes a node from the scene graph (async)
        void removeNode(
            vsg::Group* parent,
            unsigned index);

        //! TODO: Signal that something has changed that requires shader regen.
        //! When we implement this, it will probably fire off a callback that
        //! signals listeners to recreate their graphics pipelines so they
        //! can incorporate the new shader settings.
        //! OR, we can just maintain a revision number and the update() pass
        //! can check it as needed.
        void dirtyShaders() {
            shaderSettingsRevision++;
        }

        //! Update any pending compile results. Returns true if updates occurred.
        bool update();

    private:
        // for (some) update operations
        vsg::ref_ptr<vsg::Operation> _priorityUpdateQueue;

        // containers for compilation and integrating the results
        mutable std::shared_mutex _compileMutex;
        std::queue<vsg::ref_ptr<vsg::Object>> _toCompile;
        std::vector<vsg::CompileResult> _compileResults;

        // deferred deletion container
        mutable std::shared_mutex _deferred_unref_mutex;
        std::list<std::vector<vsg::ref_ptr<vsg::Object>>> _deferred_unref_queue;
    };
}

