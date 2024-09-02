/**
 * rocky c++
 * Copyright 2024 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/engine/ViewLocal.h>
#include <rocky/EntityPosition.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/Transform.h>

namespace ROCKY_NAMESPACE
{
    template<class T>
    struct TerrainRelativePositionedObjectAdapter : public TerrainRelativePositionedObject
    {
        vsg::ref_ptr<T> object;
        virtual const EntityPosition& objectPosition() const {
            return object->position;
        }
        static std::shared_ptr<TerrainRelativePositionedObjectAdapter<T>> create(vsg::ref_ptr<T> object_) {
            auto r = std::make_shared< TerrainRelativePositionedObjectAdapter<T>>();
            r->object = object_;
            return r;
        }
    };
    /**
     * Transform node that accepts geospatial coordinates and creates
     * a local ENU (X=east, Y=north, Z=up) coordinate frame for its children
     * that is tangent to the earth at the transform's geo position on the 
     * terrain surface.
     */
    class ROCKY_EXPORT EntityTransform :
        public vsg::Inherit<vsg::Group, EntityTransform>,
        TerrainRelativePositionedObject
    {
    public:
        EntityPosition position;

        //! Sphere for horizon culling
        vsg::dsphere bound = { };

        //! whether horizon culling is active
        bool horizonCulling = true;

    public:
        //! Construct an invalid EntityTransform
        EntityTransform();

        //! Call this is you change position directly.
        void dirty();

        //! Same as changing position and calling dirty().
        void setPosition(const EntityPosition& p);

    public: // TerrainRelativePositionedObject interface

        const EntityPosition& objectPosition() const override {
            return position;
        }

    public:

        EntityTransform(const EntityTransform& rhs) = delete;

        void accept(vsg::RecordTraversal&) const override;

        bool push(vsg::RecordTraversal&, const vsg::dmat4& m) const;

        void pop(vsg::RecordTraversal&) const;

    protected:


        struct Data {
            bool dirty = true;
            GeoPoint worldPos;
            vsg::dmat4 matrix;
            vsg::dmat4 local_matrix;
        };
        util::ViewLocal<Data> _viewlocal;

    };
} // namespace
