/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Map.h>
#include <rocky/TerrainTileModel.h>
#include <rocky/Threading.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>

namespace rocky
{
    class CreateTileManifest;
    class TerrainContext;
    class TerrainTileNode;
    class TerrainTileModel;

    /**
     * Handles the loading of data of an individual tile node
     */
    class LoadTileDataOperation
    {
    public:
        using Ptr = shared_ptr<LoadTileDataOperation>;

        LoadTileDataOperation(
            shared_ptr<Map> map,
            TerrainTileNode* tilenode);

        //! New tile data request for a subset of layers (in the manifest)
        LoadTileDataOperation(
            shared_ptr<Map> map,
            const CreateTileManifest& manifest,
            TerrainTileNode* tilenode);

        virtual ~LoadTileDataOperation();

        //! Whether to allow the request to cancel midstream. Default is true
        void setEnableCancelation(bool value) { 
            _enableCancel = value;
        }

        //! Dispatch the job.
        bool dispatch(bool async = true);

        //! Merge the results into the TileNode
        bool merge(shared_ptr<TerrainContext> context);

        util::Future<TerrainTileModel> _result;
        CreateTileManifest _manifest;
        bool _enableCancel;
        weak_ptr<Map> _map;
        vsg::observer_ptr<TerrainTileNode> _tilenode;
        std::string _name;
        bool _dispatched;
        bool _merged;
    };
}
