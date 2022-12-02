/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/LoadTileData.h>

#include <vsg/nodes/Node.h>

namespace rocky
{
    /**
     * A queue that merges new tile data into the respective tile.
     */
    class Merger
    {
    public:
        //! Construct a new merger
        Merger();

        //! Maximum number of merges to perform per UPDATE frame
        //! Default = unlimited
        void setMergesPerFrame(unsigned value);

        //! clear it
        void clear();

        //! Queue up some load results for later merge (on update)
        void merge(
            LoadTileDataOperation::Ptr data);

        //! Call this once and a while to get tiles merged
        void update(
            TerrainContext& terrain);


        //void traverse(osg::NodeVisitor& nv) override;

        //void releaseGLObjects(osg::State* state) const override;

    private:

        // GL objects compile task
        struct ToCompile {
            std::shared_ptr<LoadTileDataOperation> _data;
            util::Future<vsg::ref_ptr<vsg::Node>> _compiled;
        };

        // Queue of GL object compilations to perform per-merge,
        // if an ICO is installed
        using CompileQueue = std::list<ToCompile>;
        CompileQueue _compileQueue;
        CompileQueue _tempQueue;

        // Queue of tile data to merge during UPDATE traversal
        using MergeQueue = std::queue<LoadTileDataOperation::Ptr>;
        MergeQueue _mergeQueue;
        //JobArena::Metrics::Arena::Ptr _metrics;

        mutable util::Mutex _mutex;
        unsigned _mergesPerFrame;

        //FrameClock _clock;
    };
}
