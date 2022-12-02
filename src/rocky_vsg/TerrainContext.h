/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky_vsg/Merger.h>
#include <rocky_vsg/SelectionInfo.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TileNodeRegistry.h>
#include <rocky_vsg/TileRenderModel.h>
#include <vsg/core/ref_ptr.h>

namespace rocky
{
    class GeometryPool;
    class Map;
    class Merger;
    class SelectionInfo;
    class TerrainNode;
    class TerrainSettings;
    class TileNodeRegistry;

    class TerrainContext : public TerrainSettings
    {
    public:
        TerrainContext(const Config& conf) :
            TerrainSettings(conf) { }

        shared_ptr<Map> map;
        //vsg::ref_ptr<TerrainNode> terrain;
        GeometryPool geometryPool;
        Merger merger;
        TileNodeRegistry tiles;
        RenderBindings bindings{ 5 };
        internal::SelectionInfo selectionInfo;
        std::string loadArenaName = "terrain.load";
        //TerrainSettings* settings;

#if 0
            const Map*                          map,
            TerrainEngineNode*                  engine,
            GeometryPool*                       geometryPool,
            Merger*                             merger,
            TileNodeRegistry::Ptr               tiles,
            const RenderBindings&               renderBindings,
            const TerrainOptions&               options,
            const SelectionInfo&                selectionInfo,
            const FrameClock*                   clock);
        
        Merger* getMerger() const { return _merger; }

        const RenderBindings& getRenderBindings() const { return _renderBindings; }

        GeometryPool* getGeometryPool() const { return _geometryPool; }

        osg::ref_ptr<const Map> getMap() const;

        // only call this variant when it's safe to do so (update, cull).
        osg::ref_ptr<TerrainEngineNode> getEngine() { return osg::ref_ptr<TerrainEngineNode>(_terrainEngine); }

        TileNodeRegistry::Ptr tiles() const { return _tiles; }

        const SelectionInfo& getSelectionInfo() const { return _selectionInfo; }

        const TerrainOptions& options() const { return _options; }

        ProgressCallback* progress() const { return _progress.get(); }

        double getExpirationRange2() const { return _expirationRange2; }

        ModifyBoundingBoxCallback* getModifyBBoxCallback() const { return _bboxCB.get(); }

        bool getUseTextureBorder() const { return false; }

        const FrameClock* getClock() const { return _clock; }

        TextureArena* textures() const { return _textures.get(); }

    protected:

        virtual ~EngineContext() { }

    public:
        
        osg::observer_ptr<TerrainEngineNode>  _terrainEngine;
        osg::observer_ptr<const Map>          _map;
        TileNodeRegistry::Ptr                 _tiles;
        const TerrainOptions&                 _options;
        const RenderBindings&                 _renderBindings;
        GeometryPool*                         _geometryPool;
        Merger*                               _merger;
        const SelectionInfo&                  _selectionInfo;
        osg::Timer_t                          _tick;
        int                                   _tilesLastCull;
        osg::ref_ptr<ProgressCallback>        _progress;    
        double                                _expirationRange2;
        osg::ref_ptr<ModifyBoundingBoxCallback> _bboxCB;
        const FrameClock*                     _clock;
        osg::ref_ptr<TextureArena>            _textures;
#endif
    };

}
