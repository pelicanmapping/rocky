/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <rocky_vsg/InstanceVSG.h>
#include <rocky/DateTime.h>
#include <rocky/SRS.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/Light.h>

namespace ROCKY_NAMESPACE
{
    //class Runtime;

    /**
    * Node that renders an atmosphere, stars, sun and moon.
    * (Note: this only works with a geocentric world SRS.)
    */
    class ROCKY_VSG_EXPORT SkyNode : public vsg::Inherit<vsg::Group, SkyNode>
    {
    public:
        //! Creates a new sky node
        SkyNode(const InstanceVSG& instance);

        //! Sets the spatial reference system of the earth (geocentric)
        void setWorldSRS(const SRS& srs);

        //! Toggle the rendering of the atmosphere
        void setShowAtmosphere(bool value);

        //! Set the date and time
        void setDateTime(const DateTime& value);

    public:
        vsg::ref_ptr<vsg::AmbientLight> ambient;
        vsg::ref_ptr<vsg::PointLight> sun;

    protected:
        vsg::ref_ptr<vsg::Node> _atmosphere;
        InstanceVSG _instance;
    };
};
