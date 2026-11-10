/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/DisplayManager.h>
#include <rocky/vsg/ecs/SlugResource.h>
#include <atomic>
#include <unordered_map>

#ifdef ROCKY_HAS_SLUGHORN
namespace ROCKY_NAMESPACE::detail
{
    //! Update-thread counters except resident bytes, whose retirement may run on the disposer thread.
    struct SlugPassStatistics
    {
        std::atomic<std::uint64_t> residentBytes{ 0 };
        std::uint64_t peakBytes = 0, uploadedBytes = 0, uploads = 0;
        std::uint64_t budgetBytes = 256ull * 1024ull * 1024ull;
    };

    //! Immutable per-payload GPU allocation, shared by all placements and views of this revision.
    struct SlugPassGeometry : vsg::Inherit<vsg::Object, SlugPassGeometry>
    {
        vsg::ref_ptr<vsg::DescriptorBuffer> buffer;
        std::shared_ptr<SlugPassStatistics> statistics;
        std::uint64_t revision = 0, bytes = 0;
        std::uint32_t count = 0, outlines = 0;
        bool segments = false;
        //! Accounts for retired storage after every view/instance has released its allocation.
        ~SlugPassGeometry() { if (statistics) statistics->residentBytes.fetch_sub(bytes); }
    };

    //! Builds one bounded immutable allocation; returns null on capacity/compile failure, leaving fallback intact.
    vsg::ref_ptr<SlugPassGeometry> createSlugPassGeometry(VSGContext, const SlugResource&,
        const std::shared_ptr<SlugPassStatistics>&);

    //! View-owned additional pass. All mutations happen during update; recording only reads the published list.
    class ROCKY_EXPORT SlugPassNode : public vsg::Inherit<vsg::Group, SlugPassNode>
    {
    public:
        //! Attaches after the main render graph; unsupported windows or missing shaders return null.
        static vsg::ref_ptr<SlugPassNode> attach(VSGContext, Window&, View&);
        //! Detaches the complete owned sequence and compiler references while the main view still exists.
        static bool detach(VSGContext, const Window&, const View&);
        //! Updates framebuffer/viewport state and begins a coherent view submission; false selects terrain fallback.
        bool begin(VSGContext);
        //! Adds a visible placement referencing shared immutable geometry; false requests whole-view fallback.
        bool add(std::uint32_t decal, vsg::ref_ptr<SlugPassGeometry>, const Color& highlight);
        //! Publishes all or none of this view's instances and retires obsolete bindings safely.
        bool finish(VSGContext, bool compatible);
        //! Follows main viewport changes, including nonzero origins.
        void refresh(const View&);
        //! Records adjacent material batches while preserving placement and outline/core order.
        void traverse(vsg::RecordTraversal&) const override;

        //! Published placement count, for update-thread diagnostics; geometry remains shared.
        std::size_t visibleInstances() const { return _draws.size(); }
        //! Published material batch count, for update-thread performance diagnostics.
        std::size_t batchCount() const { return _batches.size(); }

        //! Diagnostic reference: use one unspecialized draw in the same submission order.
        bool preserveSubmissionOrder = false;
        bool active = false;
        std::string reason = "preparing";

    private:
        struct Instance
        {
            vsg::ref_ptr<SlugPassGeometry> geometry;
            std::uint32_t decal = 0;
            Color highlight = Color(0, 0, 0, 0);
        };
        //! Reads/compiles the pass shaders and builds the compatible pipeline before any routing changes.
        bool initialize(VSGContext);
        //! Rebuilds capacity-dependent layouts after Application finalizes its startup descriptor capacity.
        bool refreshCapacity(VSGContext);
        std::uint32_t _capacity = 0;
        vsg::observer_ptr<vsg::Window> _window;
        vsg::observer_ptr<vsg::View> _mainView;
        vsg::observer_ptr<vsg::RenderGraph> _mainGraph;
        std::string _reportedReason;
        vsg::observer_ptr<vsg::RenderGraph> _graph;
        vsg::observer_ptr<vsg::View> _view;
        vsg::ref_ptr<vsg::PipelineBarrier> _barrier;
        vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> _config;
        vsg::ref_ptr<vsg::Sampler> _sampler;
        vsg::ref_ptr<vsg::ImageView> _depth;
        vsg::ref_ptr<vsg::DescriptorImage> _depthDescriptor;
        vsg::ref_ptr<vsg::vec4Array> _camera;
        vsg::ref_ptr<vsg::DescriptorBuffer> _cameraDescriptor;
        vsg::ref_ptr<vsg::BindDescriptorSet> _atlas;
        Revision _sharedRevision = -1;
        std::uint64_t _frame = 0;
        bool _prepared = false;
        std::weak_ptr<SharedRenderData> _shared;
        std::weak_ptr<SlugPassPolicy> _policy;
        std::vector<Instance> _pending, _draws;
        vsg::ref_ptr<vsg::ubyteArray> _spans;
        vsg::ref_ptr<vsg::DescriptorBuffer> _spanDescriptor;
        vsg::ref_ptr<vsg::BindDescriptorSet> _local;
        std::vector<vsg::ref_ptr<SlugPassGeometry>> _boundGeometry;
        std::uint32_t _instanceCount = 0;
        struct Batch { std::uint32_t first = 0, count = 0; bool segments = false; };
        std::vector<Batch> _batches;
        //! Publishes small per-view placement/range metadata; immutable payload buffers are never repacked.
        bool prepareBatch(VSGContext);
    };
}

#endif // ROCKY_HAS_SLUGHORN
