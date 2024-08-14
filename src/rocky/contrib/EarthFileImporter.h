/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Imports data from osgEarth ".earth" files.
     */
    class ROCKY_EXPORT EarthFileImporter
    {
    public:
        //! Construct a new importer
        EarthFileImporter();

        //! Read an earth file and parse it into JSON data.
        //! This JSON string is suitable for reading into a Map object. 
        //! While the importer will read everything in the earthfile, not all contents
        //! are supported by Rocky.
        Result<std::string> read(const std::string& location, const IOOptions& io) const;
    };
}
