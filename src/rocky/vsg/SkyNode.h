/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/DateTime.h>
#include <rocky/SRS.h>

#if VSG_API_VERSION_GREATER_EQUAL(1,1,3)
#include <vsg/lighting/AmbientLight.h>
#include <vsg/lighting/PointLight.h>
#else
#include <vsg/nodes/Light.h>
#endif

namespace ROCKY_NAMESPACE
{
    /**
    * Node that renders an atmosphere, stars, sun and moon.
    * (Note: this only works with a geocentric world SRS.)
    */
    class ROCKY_EXPORT SkyNode : public vsg::Inherit<vsg::Group, SkyNode>
    {
    public:
        //! Creates a new sky node
        SkyNode(const VSGContext instance);

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
        VSGContext _context;
    };
};
