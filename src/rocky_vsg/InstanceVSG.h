/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <rocky_vsg/Runtime.h>
#include <rocky/Instance.h>
#include <vsg/io/Options.h>
#include <vsg/utils/CommandLine.h>

#include <rocky_vsg/LineState.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Rocky instance to use when running a VSG-based application
     */
    class ROCKY_VSG_EXPORT InstanceVSG : public Instance
    {
    public:
        //! Construct a new VSG-based application instance
        InstanceVSG();

        //! Construct a new VSG-based application instance
        //! @param args Command line arguments to parse
        InstanceVSG(vsg::CommandLine& args);

        //! Copy constructor
        InstanceVSG(const InstanceVSG& rhs);

        //! Runtime context
        inline Runtime& runtime();

        //! Whether to redirect rocky::Log messages to the vsg::Logger
        void setUseVSGLogger(bool);

    private:
        struct Implementation
        {
            Runtime runtime;
            engine::LineState lineState;
        };
        std::shared_ptr<Implementation> _impl;
        friend class Application;
    };


    Runtime& InstanceVSG::runtime() {
        return _impl->runtime;
    }
}
