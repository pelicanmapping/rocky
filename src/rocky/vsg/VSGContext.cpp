/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "VSGContext.h"
#include "VSGUtils.h"
#include <rocky/Image.h>
#include <rocky/URI.h>
#include <rocky/GeoExtent.h>
#include <filesystem>

#include <spdlog/sinks/stdout_color_sinks.h>

ROCKY_ABOUT(vulkanscenegraph, VSG_VERSION_STRING)

#ifdef ROCKY_HAS_VSGXCHANGE
#include <vsgXchange/all.h>
ROCKY_ABOUT(vsgxchange, VSGXCHANGE_VERSION_STRING)
#endif

#ifdef ROCKY_HAS_GDAL
#include <rocky/GDAL.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    // custom VSG logger that redirects to spdlog.
    class VSG_to_Spdlog_Logger : public Inherit<vsg::Logger, VSG_to_Spdlog_Logger>
    {
    public:
        std::shared_ptr<spdlog::logger> vsg_logger;

        VSG_to_Spdlog_Logger()
        {
            vsg_logger = spdlog::get("vsg");
            if (!vsg_logger)
            {
                vsg_logger = spdlog::stdout_color_mt("vsg");
                vsg_logger->set_pattern("%^[%n %l]%$ %v");
            }
        }

    protected:
        const char* ignore = "[rocky.ignore]";

        void debug_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->debug(message);
            }
        }
        void info_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->info(message);
            }
        }
        void warn_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->warn(message);
            }
        }
        void error_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->error(message);
            }
        }
        void fatal_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->critical(message);
            }
        }
    };

    // recursive search for a vsg::ReaderWriters that matches the extension
    // TODO: expand to include 'protocols' I guess
    vsg::ref_ptr<vsg::ReaderWriter> findReaderWriter(const std::string& extension, const vsg::ReaderWriters& readerWriters)
    {
        vsg::ref_ptr<vsg::ReaderWriter> output;

        for (auto& rw : readerWriters)
        {
            vsg::ReaderWriter::Features features;
            auto crw = dynamic_cast<vsg::CompositeReaderWriter*>(rw.get());
            if (crw)
            {
                output = findReaderWriter(extension, crw->readerWriters);
            }
            else if (rw->getFeatures(features))
            {
                auto j = features.extensionFeatureMap.find(extension);

                if (j != features.extensionFeatureMap.end())
                {
                    if (j->second & vsg::ReaderWriter::FeatureMask::READ_ISTREAM)
                    {
                        output = rw;
                    }
                }
            }

            if (output)
                break;
        }

        return output;
    }

#ifdef ROCKY_HAS_GDAL
    /**
    * VSG reader-writer that uses GDAL to read some image formats that are
    * not supported by vsgXchange
    */
    class GDAL_VSG_ReaderWriter : public vsg::Inherit<vsg::ReaderWriter, GDAL_VSG_ReaderWriter>
    {
    public:
        Features _features;

        GDAL_VSG_ReaderWriter()
        {
            _features.extensionFeatureMap[vsg::Path(".webp")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".tif")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".jpg")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".png")] = READ_ISTREAM;
        }

        bool getFeatures(Features& out) const override
        {
            out = _features;
            return true;
        }

        vsg::ref_ptr<vsg::Object> read(std::istream& in, vsg::ref_ptr<const vsg::Options> options = {}) const override
        {
            if (!options || _features.extensionFeatureMap.count(options->extensionHint) == 0)
                return {};

            std::stringstream buf;
            buf << in.rdbuf() << std::flush;
            std::string data = buf.str();

            std::string gdal_driver =
                options->extensionHint.string() == ".webp" ? "webp" :
                options->extensionHint.string() == ".tif" ? "gtiff" :
                options->extensionHint.string() == ".jpg" ? "jpeg" :
                options->extensionHint.string() == ".png" ? "png" :
                "";

            auto result = GDAL_detail::readImage((unsigned char*)data.c_str(), data.length(), gdal_driver);

            if (result.ok())
                return util::moveImageToVSG(result.value());
            else
                return { };
        }
    };
#endif

    std::string inferContentTypeFromStream(std::istream& stream)
    {
        // Get the length of the stream
        stream.seekg(0, std::ios::end);
        unsigned int len = stream.tellg();
        stream.seekg(0, std::ios::beg);

        if (len < 16) return {};

        // Read a 16 byte header
        char data[16];
        stream.read(data, 16);

        // Reset reading
        stream.seekg(0, std::ios::beg);

        return URI::inferContentType(std::string(data, 16));
    }

    bool foundShaders(const vsg::Paths& searchPaths)
    {
        auto options = vsg::Options::create();
        options->paths = searchPaths;
        auto found = vsg::findFile(vsg::Path("shaders/rocky.terrain.vert"), options);
        return !found.empty();
    }    
    
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

    struct SimpleUpdateOperation : public vsg::Inherit<vsg::Operation, SimpleUpdateOperation>
    {
        std::function<void()> _function;

        SimpleUpdateOperation(std::function<void()> function) :
            _function(function) { }

        void run() override
        {
            _function();
        };
    };
}




VSGContextImpl::VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer) :
    rocky::ContextImpl(),
    _viewer(viewer)
{
    if (!_viewer) _viewer = vsg::Viewer::create();
    int argc = 0;
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

VSGContextImpl::VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer, int& argc, char** argv) :
    rocky::ContextImpl(),
    _viewer(viewer)
{
    if (!_viewer) _viewer = vsg::Viewer::create();
    ctor(argc, argv);
}

void
VSGContextImpl::ctor(int& argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    readerWriterOptions = vsg::Options::create();

    shaderCompileSettings = vsg::ShaderCompileSettings::create();

    _priorityUpdateQueue = PriorityUpdateQueue::create();

    // initialize the deferred deletion collection.
    // a large number of frames ensures objects will be safely destroyed and
    // and we won't have too many deletions per frame.
    _gc.resize(8);

    // big capacity for this so we can copy it without worry about reallocating.
    activeViewIDs.reserve(128);

    args.read(readerWriterOptions);

    // redirect the VSG logger to our spdlog
    vsg::Logger::instance() = new VSG_to_Spdlog_Logger();

    // set the logging level from the command line
    std::string log_level;
    if (args.read("--log-level", log_level))
    {
        if (log_level == "debug") Log()->set_level(spdlog::level::debug);
        else if (log_level == "info") Log()->set_level(spdlog::level::info);
        else if (log_level == "warn") Log()->set_level(spdlog::level::warn);
        else if (log_level == "error") Log()->set_level(spdlog::level::err);
        else if (log_level == "critical") Log()->set_level(spdlog::level::critical);
        else if (log_level == "off") Log()->set_level(spdlog::level::off);
    }

#ifdef ROCKY_HAS_GDAL
    readerWriterOptions->add(GDAL_VSG_ReaderWriter::create());
#endif

#ifdef ROCKY_HAS_VSGXCHANGE
    // Adds all the readerwriters in vsgxchange to the options data.
    readerWriterOptions->add(vsgXchange::all::create());
#endif

    // For system fonts
    readerWriterOptions->paths.push_back("C:/Windows/Fonts");
    readerWriterOptions->paths.push_back("/usr/share/fonts/truetype");
    readerWriterOptions->paths.push_back("/etc/fonts");
    readerWriterOptions->paths.push_back("/usr/local/share/rocky/data");

    // establish search paths for shaders and data:
    auto vsgPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    searchPaths.insert(searchPaths.end(), vsgPaths.begin(), vsgPaths.end());

    auto rockyPaths = vsg::getEnvPaths("ROCKY_FILE_PATH");
    searchPaths.insert(searchPaths.end(), rockyPaths.begin(), rockyPaths.end());

    // add some default places to look for shaders and resources, relative to the executable.
    const char* relative_paths_to_add[] = {
        "../share/rocky",                        // running from standard install location
        "../../../../../src/rocky/vsg",          // running from visual studio with build folder inside repo
        "../../../../../repo/src/rocky/vsg",     // running from visual studio with a repo folder :)
        "../../../../src/rocky/vsg"              // running from visual studio with an in-source build :(
    };

    auto exec_path = std::filesystem::path(util::getExecutableLocation());
    Log()->info("Running from: {}", exec_path.string());

    for (auto& relative_path : relative_paths_to_add)
    {
        auto path = (exec_path.remove_filename() / relative_path).lexically_normal();
        if (!path.empty())
            searchPaths.push_back(vsg::Path(path.generic_string()));
    }

    searchPaths.emplace_back("/usr/local/share/rocky");

    if (!foundShaders(searchPaths))
    {
        Log()->warn("Trouble: Rocky may not be able to find its shaders. "
            "Consider setting one of the environment variables VSG_FILE_PATH or ROCKY_FILE_PATH.");

        status = Failure(Failure::ResourceUnavailable, "Cannot find shaders - check your ROCKY_FILE_PATH");
        return;
    }

    Log()->debug("Search paths:");
    for (auto& path : searchPaths)
        Log()->debug("  {}", path.string());

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.
    io.services().readImageFromURI = [](const std::string& location, const rocky::IOOptions& io)
    {
        auto result = URI(location).read(io);
        if (result.ok())
        {
            std::istringstream buf(result.value().content.data);
            return io.services().readImageFromStream(buf, result.value().content.type, io);
        }
        return Result<std::shared_ptr<Image>>(Failure(Failure::ResourceUnavailable, "Data is null"));
    };

    // map of mime-types to extensions that VSG can understand
    static const std::unordered_map<std::string, std::string> ext_for_mime_type = {
        { "image/bmp", ".bmp" },
        { "image/gif", ".gif" },
        { "image/jpg", ".jpg" },
        { "image/jpeg", ".jpg" },
        { "image/png", ".png" },
        { "image/tga", ".tga" },
        { "image/tif", ".tif" },
        { "image/tiff", ".tif" },
        { "image/webp", ".webp" }
    };

    // To read from a stream, we have to search all the VS readerwriters to
    // find one that matches the 'extension' we want. We also have to put that
    // extension in the options structure as a hint.
    io.services().readImageFromStream = [options(readerWriterOptions)](std::istream& location, std::string contentType, const rocky::IOOptions& io) -> Result<std::shared_ptr<Image>>
        {
            // try the mime-type mapping:
            auto i = ext_for_mime_type.find(contentType);
            if (i != ext_for_mime_type.end())
            {
                auto rw = findReaderWriter(i->second, options->readerWriters);
                if (rw != nullptr)
                {
                    auto local_options = vsg::Options::create(*options);
                    local_options->extensionHint = i->second;
                    auto result = rw->read_cast<vsg::Data>(location, local_options);
                    return util::makeImageFromVSG(result);
                }
            }

            // mime-type didn't work; try the content type directly as an extension
            if (!contentType.empty())
            {
                auto contentTypeAsExtension = contentType[0] != '.' ? ("." + contentType) : contentType;
                auto rw = findReaderWriter(contentTypeAsExtension, options->readerWriters);
                if (rw != nullptr)
                {
                    auto local_options = vsg::Options::create(*options);
                    local_options->extensionHint = contentTypeAsExtension;
                    auto result = rw->read_cast<vsg::Data>(location, local_options);
                    return util::makeImageFromVSG(result);
                }
            }

            // last resort, try checking the data itself
            // TODO: maybe this should be a FIRST resort?
            auto inferredContentType = inferContentTypeFromStream(location);
            if (!inferredContentType.empty())
            {
                auto i = ext_for_mime_type.find(inferredContentType);
                if (i != ext_for_mime_type.end())
                {
                    auto rw = findReaderWriter(i->second, options->readerWriters);
                    if (rw != nullptr)
                    {
                        auto local_options = vsg::Options::create(*options);
                        local_options->extensionHint = i->second;
                        auto result = rw->read_cast<vsg::Data>(location, local_options);
                        return util::makeImageFromVSG(result);
                    }
                }
            }

            return Failure(Failure::ServiceUnavailable, "No image reader for \"" + contentType + "\"");
        };

    // caches URI request results
    io.services().contentCache = std::make_shared<ContentCache>(256);

    // weak cache of resident image (and elevation) rasters
    io.services().residentImageCache = std::make_shared<util::ResidentCache<std::string, Image, GeoExtent>>();

    // remembers failed URI requests so we don't repeat them
    io.services().deadpool = std::make_shared<DealpoolService>(4096);
}

vsg::ref_ptr<vsg::Device>
VSGContextImpl::device()
{
    return _viewer->windows().size() > 0 ? _viewer->windows().front()->getOrCreateDevice() : nullptr;
}

VulkanExtensions*
VSGContextImpl::ext()
{
    if (!_vulkanExtensions)
    {
        std::scoped_lock lock(_compileMutex);
        if (!_vulkanExtensions)
        {
            ROCKY_HARD_ASSERT(device());
            _vulkanExtensions = VulkanExtensions::create(device());
        }
    }
    return _vulkanExtensions;
}

vsg::ref_ptr<vsg::CommandGraph>
VSGContextImpl::getComputeCommandGraph() const
{
    return _computeCommandGraph;
}

vsg::ref_ptr<vsg::CommandGraph>
VSGContextImpl::getOrCreateComputeCommandGraph(vsg::ref_ptr<vsg::Device> device, int queueFamily)
{
    if (!_computeCommandGraph && device)
    {
        _computeCommandGraph = vsg::CommandGraph::create(device, queueFamily);
    }
    return _computeCommandGraph;
}

void
VSGContextImpl::onNextUpdate(vsg::ref_ptr<vsg::Operation> function, std::function<float()> get_priority)
{
    auto pq = dynamic_cast<PriorityUpdateQueue*>(_priorityUpdateQueue.get());
    if (pq)
    {
        std::scoped_lock lock(pq->_mutex);

        if (pq->referenceCount() == 1)
        {
            _viewer->updateOperations->add(_priorityUpdateQueue, vsg::UpdateOperations::ALL_FRAMES);
        }

        pq->_queue.push_back({ function, get_priority });

        requestFrame();
    }
}

void
VSGContextImpl::onNextUpdate(std::function<void()> function)
{
    _viewer->updateOperations->add(SimpleUpdateOperation::create(function));

    requestFrame();
}

vsg::CompileResult
VSGContextImpl::compile(vsg::ref_ptr<vsg::Object> compilable)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(compilable.valid(), {});

    // note: this can block (with a fence) until a compile traversal is available.
    // Be sure to group as many compiles together as possible for maximum performance.
    auto cr = _viewer->compileManager->compile(compilable);

    if (cr)
    {
        // compile results are stored and processed later during update
        std::unique_lock lock(_compileMutex);
        _compileResult.add(cr);
    }

    return cr;
}

void
VSGContextImpl::dispose(vsg::ref_ptr<vsg::Object> object)
{
    if (object)
    {
        // if the user installed a custom disposer, use it
        if (disposer)
        {
            disposer(object);
        }

        // otherwise use our own
        else
        {
            std::unique_lock lock(_gc_mutex);
            _gc.back().emplace_back(object);
        }

        requestFrame();
    }
}

void
VSGContextImpl::upload(const vsg::BufferInfoList& bufferInfos)
{
    // A way to upload GPU buffers without using the dirty()/DYNAMIC_DATA mechanism,
    // which gets slow with a large number of buffers.
    // inspired by: https://github.com/vsg-dev/VulkanSceneGraph/discussions/1572
    vsg::BufferInfoList validBufferInfos;
    validBufferInfos.reserve(bufferInfos.size());

    for (auto& bi : bufferInfos)
    {
        if (bi && bi->data)
        {
            bi->data->dirty();
            validBufferInfos.emplace_back(bi);
        }
    }

    if (!validBufferInfos.empty())
    {
        auto& tasks = _viewer->recordAndSubmitTasks;
        for (auto& task : tasks)
        {
            task->transferTask->assign(validBufferInfos);
        }

        requestFrame();
    }
}

void
VSGContextImpl::upload(const vsg::ImageInfoList& imageInfos)
{
    // A way to upload images without using the dirty()/DYNAMIC_DATA mechanism,
    // which gets slow with a large number of buffers.
    // inspired by: https://github.com/vsg-dev/VulkanSceneGraph/discussions/1572
    vsg::ImageInfoList validImageInfos;
    validImageInfos.reserve(imageInfos.size());
    for (auto& bi : imageInfos)
    {
        if (bi && bi->imageView && bi->imageView->image && bi->imageView->image->data)
        {
            bi->imageView->image->data->dirty();
            validImageInfos.emplace_back(bi);
        }
    }

    if (!validImageInfos.empty())
    {
        auto& tasks = _viewer->recordAndSubmitTasks;
        for (auto& task : tasks)
        {
            task->transferTask->assign(validImageInfos);
        }

        requestFrame();
    }
}

void
VSGContextImpl::requestFrame()
{
    ++renderRequests;
}

bool
VSGContextImpl::update()
{
    bool updates_occurred = false;

    // Context update callbacks
    onUpdate.fire();

    if (_compileResult)
    {
        std::unique_lock lock(_compileMutex);

        if (_compileResult.requiresViewerUpdate())
        {
            vsg::updateViewer(*_viewer, _compileResult);
            updates_occurred = true;
        }
        _compileResult.reset();

        requestFrame();
    }

    // process the garbage collector
    {
        std::unique_lock lock(_gc_mutex);
        // unref everything in the oldest collection:
        _gc.front().clear();
        // move the empty collection to the back:
        _gc.emplace_back(std::move(_gc.front()));
        _gc.pop_front();
    }

    return updates_occurred;
}
