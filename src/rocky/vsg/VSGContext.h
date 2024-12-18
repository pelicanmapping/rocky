/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Context.h>
#include <rocky/vsg/Common.h>
#include <vsg/all.h>
#include <deque>
#include <vector>

namespace ROCKY_NAMESPACE
{
    class MapNode;

    /**
     * Rocky runtime context to use with a VSG-based application.
     * Use VSGContextFactory::create to VSGContext instance.
     */
    class ROCKY_EXPORT VSGContextImpl : public ContextImpl
    {
    public:
        //! VSG viewer
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
        //! When true, the viewer will only paint a frame when requested to do so
        //! by setting renderRequests to a non-zero value or by calling requestFrame().
        bool renderOnDemand = true;

        //! Number of render-on-demand requests
        std::atomic_int renderRequests = { 0 };

        //! Request a frame render. Thread-safe.
        void requestFrame();

        //! Whether rendering is enabled in the current frame.
        bool renderingEnabled = true;

        //! Shared shader compile settings. Use this to insert shader defines
        //! that should be used throughout the application; things like enabling
        //! lighting, debug visuals, etc.
        vsg::ref_ptr<vsg::ShaderCompileSettings> shaderCompileSettings;

        //! Revision number associated with the compile settings. A clients can
        //! poll this to see if it needs to regenerate its pipeline.
        Revision shaderSettingsRevision = 0;

        //! Custom vsg object disposer (optional)
        //! By default Runtime uses its own round-robin object disposer
        std::function<void(vsg::ref_ptr<vsg::Object>)> disposer;

        //! List of viewIDs that were detected during the latest record traversal.
        //! This needs to be cleared and rebuilt every frame.
        std::vector<std::uint32_t> activeViewIDs;

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
        void onNextUpdate(std::function<void()> function);

        //! Compiles the Vulkan primitives for an object. This is a thread-safe
        //! operation. Each call to compile() might block the viewer to access
        //! a compile manager, so it is always a good idea to batch together as
        //! many compile operations as possible (e.g., with vsg::Objects) for good
        //! performance.
        void compile(vsg::ref_ptr<vsg::Object> object);

        //! Destroys a VSG object, eventually. 
        //! Call this to get rid of descriptor sets you plan to replace.
        //! You can't just let them go since they recycle internally and 
        //! you could end up trying to destroy a vulkan object while
        //! compiling new objects, which will result in a validation error
        //! and leaked memory.
        //! https://github.com/vsg-dev/VulkanSceneGraph/discussions/949
        void dispose(vsg::ref_ptr<vsg::Object> object);

        //! TODO: Signal that something has changed that requires shader regen.
        //! When we implement this, it will probably fire off a callback that
        //! signals listeners to recreate their graphics pipelines so they
        //! can incorporate the new shader settings.
        //! OR, we can just maintain a revision number and the update() pass
        //! can check it as needed.
        void dirtyShaders();

        //! Update any pending compile results. Returns true if updates occurred.
        bool update();

    private:
        // for (some) update operations
        vsg::ref_ptr<vsg::Operation> _priorityUpdateQueue;

        // containers for compilation and integrating the results
        struct ToCompile
        {
            vsg::ref_ptr<vsg::Objects> objects = vsg::Objects::create();
            std::vector<jobs::future<bool>> results;
        };
        ToCompile _toCompile;

        mutable std::mutex _compileMutex;
        vsg::CompileResult _compileResult;

        // deferred deletion container (garbage collector)
        mutable std::mutex _gc_mutex;
        std::deque<std::vector<vsg::ref_ptr<vsg::Object>>> _gc;

        jobs::future<bool> _entityJobCompiler;

    private:
        //! Construct a new VSG-based application instance
        VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer);

        //! Construct a new VSG-based instance with the given cmdline args
        VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer, int& argc, char** argv);

        //! Disable copy/move constructors
        VSGContextImpl(const VSGContextImpl& rhs) = delete;
        VSGContextImpl(VSGContextImpl&& rhs) noexcept = delete;

        void ctor(int& argc, char** argv);

        friend class Application;
        friend class VSGContextFactory;
    };

    using VSGContext = std::shared_ptr<VSGContextImpl>;

    class VSGContextFactory
    {
    public:
        template<typename... Args>
        static VSGContext create(Args&&... args) {
            return VSGContext(new VSGContextImpl(std::forward<Args>(args)...));
        }
    };
}