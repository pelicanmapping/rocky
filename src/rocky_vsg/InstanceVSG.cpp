/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "InstanceVSG.h"
#include "Utils.h"
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
}

InstanceVSG::InstanceVSG() :
    rocky::Instance()
{
    _impl = std::make_shared<Implementation>();
    _impl->vsgOptions = vsg::Options::create();

#ifdef ROCKY_SUPPORTS_GDAL
    _impl->vsgOptions->add(GDAL_VSG_ReaderWriter::create());
#endif

#ifdef VSGXCHANGE_FOUND
    // Adds all the readerwriters in vsgxchange to the options data.
    _impl->vsgOptions->add(vsgXchange::all::create());
#endif

    // a copy of vsgOptions we can use in lamdbas
    auto vsgOptions = _impl->vsgOptions;

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.

    ioOptions().services().readImageFromURI = [vsgOptions](
        const std::string& location, const rocky::IOOptions& io)
    {
        auto result = vsg::read_cast<vsg::Data>(location, vsgOptions);
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
    ioOptions().services().readImageFromStream = [vsgOptions](
        std::istream& location, const std::string& contentType, const rocky::IOOptions& io)
        -> Result<shared_ptr<Image>>
    {
        auto i = ext_for_mime_type.find(contentType);
        if (i != ext_for_mime_type.end())
        {
            auto rw = findReaderWriter(i->second, vsgOptions->readerWriters);
            if (rw != nullptr)
            {
                auto local_options = vsg::Options::create(*vsgOptions);
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
    args.read(_impl->vsgOptions);
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
