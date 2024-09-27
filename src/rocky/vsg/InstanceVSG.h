/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Instance.h>
#include <rocky/vsg/Common.h>
#include <vsg/io/Options.h>
#include <vsg/utils/CommandLine.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
     * Rocky instance to use when running a VSG-based application
     */
    class ROCKY_EXPORT InstanceVSG : public Instance
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
        Runtime& runtime();

        //! Whether to render only when a frame is requested by calling requestFrame()
        bool& renderOnDemand();

        //! Request that the system render a new frame.
        //! This only applies when renderOnDemand() == true.
        void requestFrame();

    private:
        struct Implementation;
        std::shared_ptr<Implementation> _impl;
        friend class Application;
    };
}
