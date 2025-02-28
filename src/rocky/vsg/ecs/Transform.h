/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/GeoTransform.h>
#include <vsg/app/RecordTraversal.h>

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
        std::optional<vsg::dmat4> localMatrix;

        //! Whether the localMatrix is relative to a local tangent plane at
        //! "position", versus a simple translated reference frame. Setting this
        //! to false will slightly improve performance.
        bool localTangentPlane = true;

        //! Underlying geotransform logic
        vsg::ref_ptr<GeoTransform> node; // todo. move this to a separate component...?

        //! Construct a transform component
        Transform()
        {
            node = GeoTransform::create();
        }

        //! Sets the geospatial position of this transform
        //! @param p The geospatial position
        void setPosition(const GeoPoint& p)
        {
            position = p;
            dirty();
        }
        
        //! Force an update of the underlying node
        void dirty()
        {
            node->setPosition(position);
            node->localTangentPlane = localTangentPlane;
        }

        //! Update the node's values for this traversal
        inline void update(vsg::RecordTraversal& rt)
        {
            node->update(rt, localMatrix);
        }

        //! Perform culling on this transform's node, pushing its matrix on the stack.
        //! @return True if the node is visible; you MUST call pop() if this returns true.
        inline bool push(vsg::RecordTraversal& rt)
        {
            bool visible = node->visible(rt.getState()->_commandBuffer->viewID);
            if (visible) 
                node->push(rt);
            return visible;
        }

        //! Pops a transform applied if push() returned true.
        inline void pop(vsg::RecordTraversal& rt)
        {
            node->pop(rt);
        }
    };
}
