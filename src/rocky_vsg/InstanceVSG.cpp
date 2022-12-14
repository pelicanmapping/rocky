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

using namespace rocky;

InstanceVSG::InstanceVSG() :
    Inherit()
{
    auto vsg_options = vsg::Options::create();

#ifdef VSGXCHANGE_FOUND
    vsg_options->add(vsgXchange::all::create());
#endif

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.
    ioOptions.services.readImage = [vsg_options](
        const std::string& location, const rocky::IOOptions& io)
    {
        auto result = vsg::read_cast<vsg::Data>(location, vsg_options);
        return makeImageFromVSG(result);
    };
}
