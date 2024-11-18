/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "InstanceVSG.h"
#include "engine/Utils.h"
#include "engine/Runtime.h"

#include <rocky/Image.h>
#include <rocky/URI.h>
#include <vsg/io/read.h>
#include <vsg/io/Logger.h>
#include <vsg/state/Image.h>
#include <vsg/io/ReaderWriter.h>

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

struct ROCKY_NAMESPACE::InstanceVSG::Implementation
{
    Runtime runtime;
};

namespace
{
    // custom VSG logger that redirects to spdlog.
    class VSG_to_Spdlog_Logger : public Inherit<vsg::Logger, VSG_to_Spdlog_Logger>
    {
    public:
        std::shared_ptr<spdlog::logger> vsg_logger;

        VSG_to_Spdlog_Logger()
        {
            vsg_logger = spdlog::stdout_color_mt("vsg");
            vsg_logger->set_pattern("%^[%n %l]%$ %v");
        }

    protected:
        void debug_implementation(const std::string_view& message) override {
            vsg_logger->set_level(Log()->level());
            vsg_logger->debug(message);
        }
        void info_implementation(const std::string_view& message) override {
            vsg_logger->set_level(Log()->level());
            vsg_logger->info(message);
        }
        void warn_implementation(const std::string_view& message) override {
            vsg_logger->set_level(Log()->level());
            vsg_logger->warn(message);
        }
        void error_implementation(const std::string_view& message) override {
            vsg_logger->set_level(Log()->level());
            vsg_logger->error(message);
        }
        void fatal_implementation(const std::string_view& message) override {
            vsg_logger->set_level(Log()->level());
            vsg_logger->critical(message);
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

            auto result = GDAL::readImage((unsigned char*)data.c_str(), data.length(), gdal_driver);

            if (result.status.ok())
                return util::moveImageToVSG(result.value);
            else
                return { };
        }
    };
#endif

    // Adapted from https://oroboro.com/image-format-magic-bytes
    std::string deduceContentTypeFromStream(std::istream& stream)
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

        // .jpg:  FF D8 FF
        // .png:  89 50 4E 47 0D 0A 1A 0A
        // .gif:  GIF87a
        //        GIF89a
        // .tiff: 49 49 2A 00
        //        4D 4D 00 2A
        // .bmp:  BM
        // .webp: RIFF ???? WEBP
        // .ico   00 00 01 00
        //        00 00 02 00 ( cursor files )
        switch (data[0])
        {
        case '\xFF':
            return (!strncmp((const char*)data, "\xFF\xD8\xFF", 3)) ? "image/jpg" : "";

        case '\x89':
            return (!strncmp((const char*)data,
                "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)) ? "image/png" : "";

        case 'G':
            return (!strncmp((const char*)data, "GIF87a", 6) ||
                !strncmp((const char*)data, "GIF89a", 6)) ? "image/gif" : "";

        case 'I':
            return (!strncmp((const char*)data, "\x49\x49\x2A\x00", 4)) ? "image/tif" : "";

        case 'M':
            return (!strncmp((const char*)data, "\x4D\x4D\x00\x2A", 4)) ? "image/tif" : "";

        case 'B':
            return ((data[1] == 'M')) ? "image/bmp" : "";

        case 'R':
            return (!strncmp((const char*)data, "RIFF", 4)) ? "image/webp" : "";
        }

        return { };
    }

    bool foundShaders(const vsg::Paths& searchPaths)
    {
        auto options = vsg::Options::create();
        options->paths = searchPaths;
        auto found = vsg::findFile(vsg::Path("shaders/rocky.terrain.vert"), options);
        return !found.empty();
    }
}

InstanceVSG::InstanceVSG() :
    rocky::Instance()
{
    int argc = 0;
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

InstanceVSG::InstanceVSG(int& argc, char** argv) :
    rocky::Instance()
{
    ctor(argc, argv);
}

void
InstanceVSG::ctor(int& argc, char** argv)
{
    _impl = std::make_shared<Implementation>();
    auto& runtime = _impl->runtime;

    vsg::CommandLine args(&argc, argv);

    args.read(_impl->runtime.readerWriterOptions);

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

    // set on-demand rendering mode from the command line
    if (args.read("--on-demand"))
    {
        runtime.renderOnDemand = true;
    }

#ifdef ROCKY_HAS_GDAL
    runtime.readerWriterOptions->add(GDAL_VSG_ReaderWriter::create());
#endif

#ifdef ROCKY_HAS_VSGXCHANGE
    // Adds all the readerwriters in vsgxchange to the options data.
    runtime.readerWriterOptions->add(vsgXchange::all::create());
#endif

    // For system fonts
    runtime.readerWriterOptions->paths.push_back("C:/windows/fonts");
    runtime.readerWriterOptions->paths.push_back("/etc/fonts");
    runtime.readerWriterOptions->paths.push_back("/usr/local/share/rocky/data");

    // Load a default font if there is one
    auto font_file = util::getEnvVar("ROCKY_DEFAULT_FONT");
    if (font_file.empty())
    {
#ifdef WIN32
        font_file = "arialbd.ttf";
#else
        font_file = "times.vsgb";
#endif
    }

    runtime.defaultFont = vsg::read_cast<vsg::Font>(font_file, runtime.readerWriterOptions);
    if (!runtime.defaultFont)
    {
        Log()->warn("Cannot load font \"" + font_file + "\"");
    }

    // establish search paths for shaders and data:
    auto vsgPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    runtime.searchPaths.insert(runtime.searchPaths.end(), vsgPaths.begin(), vsgPaths.end());

    auto rockyPaths = vsg::getEnvPaths("ROCKY_FILE_PATH");
    runtime.searchPaths.insert(runtime.searchPaths.end(), rockyPaths.begin(), rockyPaths.end());

    // add some default places to look for shaders and resources, relative to the executable.
    auto exec_path = std::filesystem::path(util::getExecutableLocation());
    auto path = (exec_path.remove_filename() / "../share/rocky").lexically_normal();
    if (!path.empty())
        runtime.searchPaths.push_back(vsg::Path(path.generic_string()));

    path = (exec_path.remove_filename() / "../../../../build_share").lexically_normal();
    if (!path.empty())
        runtime.searchPaths.push_back(vsg::Path(path.generic_string()));

    if (!foundShaders(runtime.searchPaths))
    {
        Log()->warn("Trouble: Rocky may not be able to find its shaders. "
            "Consider setting one of the environment variables VSG_FILE_PATH or ROCKY_FILE_PATH.");
        Log()->warn("Places I looked for a 'shaders' folder:");
        for (auto& path : runtime.searchPaths)
            Log()->warn("  {}", path.string());
    }

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.
    io().services.readImageFromURI = [](const std::string& location, const rocky::IOOptions& io)
    {
        auto result = URI(location).read(io);
        if (result.status.ok())
        {
            std::stringstream buf(result.value.data);
            return io.services.readImageFromStream(buf, result.value.contentType, io);
        }
        return Result<std::shared_ptr<Image>>(Status(Status::ResourceUnavailable, "Data is null"));
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
    io().services.readImageFromStream = [options(runtime.readerWriterOptions)](std::istream& location, std::string contentType, const rocky::IOOptions& io) -> Result<shared_ptr<Image>>
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
            auto decudedContentType = deduceContentTypeFromStream(location);
            if (!decudedContentType.empty())
            {
                auto i = ext_for_mime_type.find(decudedContentType);
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

            return Status(Status::ServiceUnavailable, "No image reader for \"" + contentType + "\"");
        };

    io().services.contentCache = std::make_shared<ContentCache>(128);

    io().uriGate = std::make_shared<util::Gate<std::string>>();
}

InstanceVSG::InstanceVSG(const InstanceVSG& rhs) :
    Instance(rhs),
    _impl(rhs._impl)
{
    //nop
}

Runtime& InstanceVSG::runtime()
{
    return _impl->runtime;
}

bool& InstanceVSG::renderOnDemand()
{
    return runtime().renderOnDemand;
}

void InstanceVSG::requestFrame()
{
    runtime().requestFrame();
}
