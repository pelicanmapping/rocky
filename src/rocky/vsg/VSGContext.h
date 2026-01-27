/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/Polyfill.h>
#include <rocky/vsg/VSGUtils.h>
#include <rocky/Context.h>
#include <rocky/Callbacks.h>
#include <rocky/Rendering.h>
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
        inline const vsg::ref_ptr<vsg::Viewer>& viewer() const;

        //! VSG object sharing
        vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

        //! VSG readerwriter options
        vsg::ref_ptr<vsg::Options> readerWriterOptions;

        //! Search paths for vsg::findFile
        vsg::Paths searchPaths;

        //! Number of render-on-demaframe rendered requests
        std::atomic_int renderRequests = { 0 };

        //! Request a frame render. Thread-safe.
        void requestFrame();

        //! Whether rendering is enabled in the current frame.
        bool renderingEnabled = true;

        //! Shared shader compile settings. Use this to insert shader defines
        //! that should be used throughout the application; things like enabling
        //! lighting, debug visuals, etc.
        vsg::ref_ptr<vsg::ShaderCompileSettings> shaderCompileSettings;

        //! Custom vsg object disposer (optional)
        //! By default Runtime uses its own round-robin object disposer
        std::function<void(vsg::ref_ptr<vsg::Object>)> disposer;

        //! List of viewIDs that are active.
        std::vector<std::uint32_t> activeViewIDs = { 0 };

        //! Callback fired during each update pass.
        Callback<> onUpdate;

        //! Callbacks to render GUI elements
        using GuiRecorder = std::function<void(RenderingState&, void* guiContext)>;
        std::deque<GuiRecorder> guiRecorders;

        //! Polyfill Vulkan Extension functions (not supplied by VSG yet)
        VulkanExtensions* ext();

        std::function<float()> devicePixelRatio = []() { return 1.0f; };

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
        //! @return the compile result, which you should use to check for errors.
        vsg::CompileResult compile(vsg::ref_ptr<vsg::Object> object);

        //! Destroys a VSG object, eventually. 
        //! Call this to get rid of descriptor sets you plan to replace.
        //! You can't just let them go since they recycle internally and 
        //! you could end up trying to destroy a vulkan object while
        //! compiling new objects, which will result in a validation error
        //! and leaked memory.
        //! https://github.com/vsg-dev/VulkanSceneGraph/discussions/949
        void dispose(vsg::ref_ptr<vsg::Object> object);

        //! Queues a bufferinfo list for transfer to the GPU. This is an alternative
        //! to marking the buffer as DYNAMIC_DATA and marking it dirty(), which
        //! is inefficient for large numbers of buffers whose data is only
        //! updated periodically.
        void upload(const vsg::BufferInfoList& bufferInfos);
        void upload(const vsg::ImageInfoList& inageInfos);

        //! The VSG/Vulkan device shared by all displays
        vsg::ref_ptr<vsg::Device> device();

        //! A command graph the application can use to run compute shaders
        vsg::ref_ptr<vsg::CommandGraph> getComputeCommandGraph() const;
        vsg::ref_ptr<vsg::CommandGraph> getOrCreateComputeCommandGraph(vsg::ref_ptr<vsg::Device> device, int queueFamily);

        //! Update any pending compile results. Returns true if updates occurred.
        bool update();

    private:
        vsg::ref_ptr<vsg::Viewer> _viewer;

        // for (some) update operations
        vsg::ref_ptr<vsg::Operation> _priorityUpdateQueue;

        mutable std::mutex _compileMutex;
        vsg::CompileResult _compileResult;

        // deferred deletion container (garbage collector)
        mutable std::mutex _gc_mutex;
        std::deque<std::vector<vsg::ref_ptr<vsg::Object>>> _gc;

        vsg::ref_ptr<vsg::CommandGraph> _computeCommandGraph;

        vsg::ref_ptr<VulkanExtensions> _vulkanExtensions;

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
        friend class MapNode;
    };

    inline const vsg::ref_ptr<vsg::Viewer>& VSGContextImpl::viewer() const {
        return _viewer;
    }

    using VSGContext = std::shared_ptr<VSGContextImpl>;

    /**
    * Factory singleton for creating a VSGContext instance.
    */
    class VSGContextFactory
    {
    public:
        template<typename... Args>
        static VSGContext create(Args&&... args) {
            return VSGContext(new VSGContextImpl(std::forward<Args>(args)...));
        }
    };
}
