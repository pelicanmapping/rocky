/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "InstanceVSG.h"
#include "engine/Utils.h"
#include <rocky/Image.h>
#include <vsg/io/read.h>
#include <vsg/io/Logger.h>
#include <vsg/state/Image.h>

#ifdef VSGXCHANGE_FOUND
#include <vsgXchange/all.h>
#endif

#ifdef ROCKY_SUPPORTS_GDAL
#include <rocky/GDAL.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
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

#ifdef ROCKY_SUPPORTS_GDAL

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
}

InstanceVSG::InstanceVSG() :
    rocky::Instance()
{
    _impl = std::make_shared<Implementation>();
    auto& runtime = _impl->runtime;

#ifdef ROCKY_SUPPORTS_GDAL
    runtime.readerWriterOptions->add(GDAL_VSG_ReaderWriter::create());
#endif

#ifdef VSGXCHANGE_FOUND
    // Adds all the readerwriters in vsgxchange to the options data.
    runtime.readerWriterOptions->add(vsgXchange::all::create());
#endif

    auto vsgPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    auto rockyPaths = vsg::getEnvPaths("ROCKY_FILE_PATH");

    if (vsgPaths.empty() && rockyPaths.empty())
    {
        Log::warn() << "Neither envivonment variable VSG_FILE_PATH nor ROCKY_FILE_PATH is set."
            " This is trouble - Rocky may not be able to find its shaders." << std::endl;
    }
    else
    {
        // Default search locations for shaders and textures:
        for (auto paths : { vsgPaths, rockyPaths })
        {
            runtime.searchPaths.insert(runtime.searchPaths.end(), paths.begin(), paths.end());
        }
    }

    // start up the state generators.
    _impl->lineState.initialize(runtime);
    ROCKY_HARD_ASSERT_STATUS(LineState::status);

    _impl->meshState.initialize(runtime);
    ROCKY_HARD_ASSERT_STATUS(MeshState::status);

    // a copy of vsgOptions we can use in lamdbas
    auto readerWriterOptions = runtime.readerWriterOptions;

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.

    ioOptions().services().readImageFromURI = [readerWriterOptions](
        const std::string& location, const rocky::IOOptions& io)
    {
        auto result = vsg::read_cast<vsg::Data>(location, readerWriterOptions);
        return util::makeImageFromVSG(result);
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
    ioOptions().services().readImageFromStream = [readerWriterOptions](
        std::istream& location, std::string contentType, const rocky::IOOptions& io)
        -> Result<shared_ptr<Image>>
    {
        if (contentType.empty())
        {
            contentType = deduceContentTypeFromStream(location);
        }

        auto i = ext_for_mime_type.find(contentType);
        if (i != ext_for_mime_type.end())
        {
            auto rw = findReaderWriter(i->second, readerWriterOptions->readerWriters);
            if (rw != nullptr)
            {
                auto local_options = vsg::Options::create(*readerWriterOptions);
                local_options->extensionHint = i->second;
                auto result = rw->read_cast<vsg::Data>(location, local_options);
                return util::makeImageFromVSG(result);
            }
        }
        return Status(Status::ServiceUnavailable, "No image reader for \"" + contentType + "\"");
    };
}

InstanceVSG::InstanceVSG(vsg::CommandLine& args) :
    InstanceVSG()
{
    args.read(_impl->runtime.readerWriterOptions);
}

InstanceVSG::InstanceVSG(const InstanceVSG& rhs) :
    Instance(rhs),
    _impl(rhs._impl)
{
    //nop
}

void
InstanceVSG::setUseVSGLogger(bool value)
{
    if (value)
    {
        Log::userFunction() = [](LogLevel level, const std::string& s)
        {
            if (level == LogLevel::INFO)
                vsg::Logger::instance()->info(s);
            else if (level == LogLevel::WARN)
                vsg::Logger::instance()->warn(s);
        };
    }
    else
    {
        Log::userFunction() = nullptr;
    }
}
