/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <rocky_vsg/RuntimeContext.h>
#include <rocky/Instance.h>
#include <vsg/io/Options.h>
#include <vsg/utils/CommandLine.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Rocky instance to use when running a VSG-based application
     */
    class ROCKY_VSG_EXPORT InstanceVSG : public Inherit<Instance, InstanceVSG>
    {
    public:
        //! Construct a new VSG-based application instance
        InstanceVSG();

        //! Construct a new VSG-based application instance
        //! @param args Command line arguments to parse
        InstanceVSG(vsg::CommandLine& args);

        //! Runtime context
        RuntimeContext& runtime() { return _runtime; }

        //! Whether to redirect rocky::Log messages to the vsg::Logger
        void setUseVSGLogger(bool);

    private:
        vsg::ref_ptr<vsg::Options> _vsgOptions;
        RuntimeContext _runtime;
    };
}
