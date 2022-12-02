/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/TerrainContext.h>

namespace rocky
{
    /**
     * Unloads terrain geometry.
     */
    class Unloader
    {
    public:
        //! Construct an unloader for a registry
        Unloader();

        //! A Tile must be at least this old before it can be removed:
        void setMaxAge(std::chrono::milliseconds value) {
            _maxAge = value; // std::max(value, 1.0);
        }
        clock::duration getMaxAge() const { 
            return _maxAge;
        }

        //! Maximum number of tiles to expire per frame
        void setMaxTilesToUnloadPerFrame(unsigned value) {
            _maxTilesToUnloadPerFrame = value;
        }
        unsigned getMaxTilesToUnloadPerFrame() const {
            return _maxTilesToUnloadPerFrame;
        }

        //! A Tile must be at least this far from the camera before it can be unloaded:
        void setMinimumRange(float value) { 
            _minRange = std::max(value, 0.0f);
        }
        float getMinimumRange() const {
            return _minRange;
        }

        //! The engine may keep at least this many tiles in memory 
        //! before disposing anything
        void setMinResidentTiles(unsigned value) {
            _minResidentTiles = value;
        }
        unsigned getMinResidentTiles() const {
            return _minResidentTiles;
        }

        //! Set the frame clock to use
        //void setFrameClock(const FrameClock* value) { _clock = value; }

    public: // Unloader

        //void unloadChildren(const std::vector<TileKey>& keys);

    public: // osg::Node
        void update(TerrainContext& terrain);

    protected:
        unsigned _minResidentTiles;
        std::chrono::milliseconds _maxAge;
        float _minRange;
        unsigned _maxTilesToUnloadPerFrame;
        std::vector<vsg::observer_ptr<TerrainTileNode>> _deadpool;
        unsigned _frameLastUpdated;
        //const FrameClock* _clock;
    };
}
