/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "InstanceVSG.h"
#include "Utils.h"
#include <rocky/Image.h>
#include <vsg/io/read.h>
#include <vsg/state/Image.h>

#ifdef VSGXCHANGE_FOUND
#include <vsgXchange/all.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    // recursive search for a RW that matches the extension
    // TODO: expand to include 'protocols' I guess
    vsg::ref_ptr<vsg::ReaderWriter> findReaderWriter(const std::string& extension, const vsg::ReaderWriters& readerWriters)
    {
        vsg::ref_ptr<vsg::ReaderWriter> output;
        vsg::ReaderWriter::Features features;

        for (auto& rw : readerWriters)
        {
            auto crw = rw->cast<vsg::CompositeReaderWriter>();
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
}

InstanceVSG::InstanceVSG() :
    Inherit()
{
    _vsgOptions = vsg::Options::create();

#ifdef VSGXCHANGE_FOUND
    _vsgOptions->add(vsgXchange::all::create());
#endif

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.
    ioOptions().services().readImageFromURI = [=](
        const std::string& location, const rocky::IOOptions& io)
    {
        auto result = vsg::read_cast<vsg::Data>(location, this->_vsgOptions);
        return makeImageFromVSG(result);
    };

    static const std::unordered_map<std::string, std::string> ext_for_mime_type = {
        { "image/bmp", ".bmp" },
        { "image/gif", ".gif" },
        { "image/jpg", ".jpg" },
        { "image/jpeg", ".jpg" },
        { "image/png", ".png" },
        { "image/tga", ".tga" },
        { "image/tif", ".tif" }
    };

    ioOptions().services().readImageFromStream = [=](
        std::istream& location, const std::string& contentType, const rocky::IOOptions& io)
        -> Result<shared_ptr<Image>>
    {
        auto i = ext_for_mime_type.find(contentType);
        if (i != ext_for_mime_type.end())
        {
            auto rw = findReaderWriter(i->second, this->_vsgOptions->readerWriters);
            if (rw != nullptr)
            {
                auto local_options = vsg::Options::create(*this->_vsgOptions);
                local_options->extensionHint = i->second;
                auto result = rw->read_cast<vsg::Data>(location, local_options);
                return makeImageFromVSG(result);
            }
        }
        return Status(Status::ServiceUnavailable, "No image reader for \"" + contentType + "\"");
    };
}

InstanceVSG::InstanceVSG(vsg::CommandLine& args) :
    InstanceVSG()
{
    args.read(_vsgOptions);
}
