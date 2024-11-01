/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/GeoTransform.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Spatial transformation component.
    * Create with 
    *   auto& transform = registry.emplace<Transform>(entity);
    * 
    * A Transform may be safely updated asynchronously.
    */
    struct Transform
    {
        //! Georeferenced position
        GeoPoint position;

        //! Local transform matrix (for rotation and scale, e.g.)
        vsg::dmat4 localMatrix;

        //! Whether the localMatrix is relative to a local tangent plane at
        //! "position", versus a simple translated reference frame.
        bool localTangentPlane = true;        

        //! Parent transform to apply before applying this one
        Transform* parent = nullptr;

        //! Underlying geotransform logic
        vsg::ref_ptr<GeoTransform> node; // todo. move this to a separate component...?

        Transform()
        {
            node = GeoTransform::create();
        }

        void setPosition(const GeoPoint& p)
        {
            position = p;
            dirty();
        }
        
        void dirty()
        {
            node->setPosition(position);
            node->localTangentPlane = localTangentPlane;
        }

        //! Applies a transformation and returns true if successful.
        //! If this method retuns true, you must issue a correspond pop() later.
        inline bool push(vsg::RecordTraversal& rt, const vsg::dmat4& m)
        {
            if (parent)
            {
                return parent->push(rt, m * localMatrix);
            }
            else
            {
                return node->push(rt, m * localMatrix);
            }
        }

        //! Pops a transform applied if push() returned true.
        inline void pop(vsg::RecordTraversal& rt)
        {
            if (parent)
            {
                parent->pop(rt);
            }
            else
            {
                node->pop(rt);
            }
        }
    };
}
