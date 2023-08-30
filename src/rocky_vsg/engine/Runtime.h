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
#include <vsg/app/Viewer.h>
#include <vsg/app/CompileManager.h>
#include <vsg/app/UpdateOperations.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/SharedObjects.h>
#include <shared_mutex>

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
     * Interface to runtime operations like the VSG compiler, thread pools,
     * shared settings, and asynchronous scene graph functions.
     */
    class ROCKY_VSG_EXPORT Runtime
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
        util::Future<vsg::ref_ptr<vsg::Font>> defaultFont;

        //! Shared shader compile settings. Use this to insert shader defines
        //! that should be used throughout the application; things like enabling
        //! lighting, debug visuals, etc.
        vsg::ref_ptr<vsg::ShaderCompileSettings> shaderCompileSettings;

        //! Revision number associated with the compile settings. A clients can
        //! poll this to see if it needs to regenerate its pipeline.
        Revision shaderSettingsRevision = 0;

        //! Queue a function to run during the update pass
        //! This is a safe way to do things that require modifying the scene
        //! or compiling vulkan objects
        void runDuringUpdate(
            vsg::ref_ptr<vsg::Operation> function,
            std::function<float()> get_priority = nullptr);

        //! Queue a function to run during the update pass.
        //! This is a safe way to do things that require modifying the scene
        //! or compiling vulkan objects
        void runDuringUpdate(
            std::function<void()> function);

        //! Function that creates a node
        using NodeFactory = std::function<vsg::ref_ptr<vsg::Node>(Cancelable&)>;

        //! Compiles an object now.
        //! Be careful to only call this from a safe thread
        void compile(vsg::ref_ptr<vsg::Object> object);

        //void compile_simple(vsg::ref_ptr<vsg::Object> object);

        //! Schedules data creation; the resulting node or nodes 
        //! get added to "parent" if the operation suceeds.
        //! Returns a future so you can check for completion.
        util::Future<bool> compileAndAddChild(
            vsg::ref_ptr<vsg::Group> parent,
            NodeFactory factory,
            const util::job& config = { });

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

        //! Update any pending compile results.
        void update();

        // Once VSG can safely handle async compilation we will
        // change this to true. See:
        // https://github.com/vsg-dev/VulkanSceneGraph/discussions/949
        bool asyncCompile = false;

    private:
        vsg::ref_ptr<vsg::Operation> _priorityUpdateQueue;

        std::shared_mutex _compileMutex;
        std::queue<vsg::ref_ptr<vsg::Object>> _toCompile;

        //std::mutex _compileResultsMutex;
        std::vector<vsg::CompileResult> _compileResults;
    };
}

