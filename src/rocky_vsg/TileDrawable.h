/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky/TileKey.h>
#include <vsg/commands/Command.h>

namespace rocky
{
    class EngineContext;
    class Image;

    /**
     * Callback that allows the layers in a Map to alter the
     * tile's perceived bounding box. This is here to support 
     * layers that may perform GPU vertex modification.
     */
    struct ModifyBoundingBoxCallback
    {
        ModifyBoundingBoxCallback(EngineContext* context);
        void operator()(const TileKey& key, Box& bbox);
        EngineContext* _context;
    };

    /**
     * TileDrawable is an osg::Drawable that represents an individual terrain tile
     * for the purposes of scene graph operations (like intersections, bounds
     * computation, statistics, etc.)
     * 
     * NOTE: TileDrawable does not actually render anything!
     * It is merely a "proxy" to support intersections, etc.
     *
     * Instead, it exposes various osg::Drawable Functors for traversing
     * the terrain's geometry. It also hold a pointer to the tile's elevation
     * raster so it can properly reflect the elevation data in the texture.
     */
    class TileDrawable : public vsg::Inherit<vsg::Command, TileDrawable>
    {
    public:
        // underlying geometry, possibly shared between this tile and other.
        vsg::ref_ptr<SharedGeometry> _geom;

        // tile dimensions
        int _tileSize;

        const TileKey _key;

        shared_ptr<Image> _elevationRaster;
        fmat4 _elevationScaleBias;

        // cached 3D mesh of the terrain tile (derived from the elevation raster)
        std::vector<fvec3> _mesh;
        Box _bboxOffsets;
        ModifyBoundingBoxCallback* _bboxCB;
        mutable float _bboxRadius;

    public:
        
        //! construct a new TileDrawable that maintains the in-memory mesh
        //! for intersection testing
        TileDrawable(
            const TileKey& key,
            vsg::ref_ptr<SharedGeometry> geometry,
            int tileSize);

        //! destructor
        virtual ~TileDrawable();

    public:

        // Sets the elevation raster for this tile
        void setElevationRaster(
            shared_ptr<Image> image,
            const fmat4& scaleBias);

        shared_ptr<Image> getElevationRaster() const {
            return _elevationRaster;
        }

        const fmat4& getElevationMatrix() const {
            return _elevationScaleBias;
        }

        // Set the render model so we can properly calculate bounding boxes
        void setModifyBBoxCallback(ModifyBoundingBoxCallback* bboxCB) { _bboxCB = bboxCB; }

        float getRadius() const { return _bboxRadius; }

        //float getWidth() const { return getBoundingBox().xmax - getBoundingBox().xmin; }

#if 0
    public: // osg::Drawable overrides

        // These methods defer functors (like stats collection) to the underlying
        // (possibly shared) geometry instance.
        bool supports(const osg::Drawable::AttributeFunctor& f) const override { return true; }
        void accept(osg::Drawable::AttributeFunctor& f) override { if ( _geom.valid() ) _geom->accept(f); }

        bool supports(const osg::Drawable::ConstAttributeFunctor& f) const override { return true; }
        void accept(osg::Drawable::ConstAttributeFunctor& f) const override { if ( _geom.valid() ) _geom->accept(f); }

        bool supports(const osg::PrimitiveFunctor& f) const override { return true; }
        void accept(osg::PrimitiveFunctor& f) const override;
        
        /** indexed functor is NOT supported since we need to apply elevation dynamically */
        bool supports(const osg::PrimitiveIndexFunctor& f) const override { return false; }


        Sphere computeBound() const override;
        Box computeBoundingBox() const override;

        void resizeGLObjectBuffers(unsigned maxsize) override;
        void releaseGLObjects(osg::State* state) const override;
#endif
    };

}

