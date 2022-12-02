/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Math.h>
#include <rocky/TileKey.h>
#include <rocky/IOTypes.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/Group.h>

#define VERTEX_VISIBLE       1 // draw it
#define VERTEX_BOUNDARY      2 // vertex lies on a skirt boundary
#define VERTEX_HAS_ELEVATION 4 // not subject to elevation texture
#define VERTEX_SKIRT         8 // it's a skirt vertex (bitmask)
#define VERTEX_CONSTRAINT   16 // part of a non-morphable constraint

namespace rocky
{
    class Map;
    class MeshEditor;
    class TerrainSettings;

    class /*internal*/ SharedGeometry :
        public vsg::Inherit<vsg::Geometry, SharedGeometry>
    {
    public:
        SharedGeometry() :
            hasConstraints(false) { }

        bool empty() const {
            return commands.empty();
        }

        bool hasConstraints;
    };


    struct GeometryKey
    {
        GeometryKey() :
            lod(-1),
            tileY(0),
            patch(false),
            size(0u)
        {
            //nop
        }

        GeometryKey(const GeometryKey& rhs) :
            lod(rhs.lod),
            tileY(rhs.tileY),
            patch(rhs.patch),
            size(rhs.size)
        {
            //nop
        }

        bool operator < (const GeometryKey& rhs) const
        {
            if (lod < rhs.lod) return true;
            if (lod > rhs.lod) return false;
            if (tileY < rhs.tileY) return true;
            if (tileY > rhs.tileY) return false;
            if (size < rhs.size) return true;
            if (size > rhs.size) return false;
            if (patch == false && rhs.patch == true) return true;
            return false;
        }

        bool operator == (const GeometryKey& rhs) const
        {
            return
                lod == rhs.lod &&
                tileY == rhs.tileY &&
                size == rhs.size &&
                patch == rhs.patch;
        }

        bool operator != (const GeometryKey& rhs) const
        {
            return
                lod != rhs.lod ||
                tileY != rhs.tileY ||
                size != rhs.size ||
                patch != rhs.patch;
        }

        // hash function for unordered_map
        std::size_t operator()(const GeometryKey& key) const
        {
            return hash_value_unsigned(
                (unsigned)key.lod,
                (unsigned)key.tileY,
                key.size,
                key.patch ? 1u : 0u);
        }

        int      lod;
        int      tileY;
        bool     patch;
        unsigned size;
    };
}

namespace std {
    // std::hash specialization for GeometryKey
    template<> struct hash<rocky::GeometryKey> {
        inline size_t operator()(const rocky::GeometryKey& key) const {
            return rocky::hash_value_unsigned(
                (unsigned)key.lod,
                (unsigned)key.tileY,
                key.size, key.patch ? 1u : 0u);
        }
    };
}


namespace rocky
{
    /**
     * Pool of terrain tile geometries.
     *
     * In a geocentric map, every tile at a particular LOD and a particular latitudinal
     * (north-south) extent shares exactly the same geometry; each tile is just shifted
     * and rotated differently. Therefore we can use the same Geometry for all tiles that
     * share the same LOD and same min/max latitude in a geocentric map. In a projected
     * map, all tiles at a given LOD share the same geometry regardless of extent, so eve
     * more sharing is possible.
     *
     * This object creates and returns geometries based on TileKeys, sharing instances
     * whenever possible. Concept adapted from OSG's osgTerrain::GeometryPool.
     */
    class GeometryPool
    {
    public:
        //! Construct the geometry pool
        GeometryPool();

        using SharedGeometries = std::unordered_map<
            GeometryKey, vsg::ref_ptr<SharedGeometry>>;

        //! Gets the Geometry associated with a tile key, creating a new one if
        //! necessary and storing it in the pool.
        //!
        void getPooledGeometry(
            const TileKey& tileKey,
            const Map* map,
            const TerrainSettings& options,
            vsg::ref_ptr<SharedGeometry>& out,
            Cancelable* state);

        //! The number of elements (incides) in the terrain skirt if applicable
        int getNumSkirtElements(
            unsigned tileSize,
            float skirtRatio) const;

        //! Are we doing pooling?
        bool isEnabled() const {
            return _enabled;
        }

        //! Clear and reset the pool
        void clear();

        //void resizeGLObjectBuffers(unsigned maxsize);
        //void releaseGLObjects(osg::State* state) const;


    public: // osg::Node

        /** Perform an update traversal to check for unused resources. */
        //void traverse(osg::NodeVisitor& nv);

        mutable util::Gate<GeometryKey> _keygate;
        mutable util::Mutex _mutex;
        SharedGeometries _sharedGeometries;
        vsg::ref_ptr<vsg::ushortArray> _defaultIndices;

        void createKeyForTileKey(
            const TileKey& tileKey,
            unsigned size,
            GeometryKey& out) const;

        vsg::ref_ptr<SharedGeometry> createGeometry(
            const TileKey& tileKey,
            const TerrainSettings& settings,
            //MeshEditor& meshEditor,
            Cancelable* progress) const;

        // builds a primitive set to use for any tile without a mask
        vsg::ref_ptr<vsg::ushortArray> createIndices(
            const TerrainSettings& settings) const;

        void tessellateSurface(
            unsigned tileSize,
            vsg::ref_ptr<vsg::ushortArray> primSet) const;

        bool _enabled;
        bool _debug;
    };

}

