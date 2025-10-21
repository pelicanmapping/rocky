/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ECS.h>
#include <rocky/vsg/VSGContext.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Base class for ECS visitors.
    */
    class ROCKY_EXPORT ECSVisitor
    {
    public:
        std::uint32_t viewID = 0;
        entt::entity currentEntity = entt::null;
        std::vector<entt::entity> collectedEntities;

        inline void reset() {
            collectedEntities.clear();
            currentEntity = entt::null;
        }

    protected:
        inline ECSVisitor(std::uint32_t in_viewID) :
            viewID(in_viewID) {
        }
    };

    /**
    * Specializes the VSG polytope intersector to locate entity components.
    */
    class ROCKY_EXPORT ECSPolytopeIntersector : public vsg::Inherit<vsg::PolytopeIntersector, ECSPolytopeIntersector>,
        public ECSVisitor
    {
    public:
        /// create intersector for a polytope with window space dimensions, projected into world coords using the Camera's projection and view matrices.
        ECSPolytopeIntersector(vsg::View* view, double xMin, double yMin, double xMax, double yMax, vsg::ref_ptr<vsg::ArrayState> initialArrayData = {}) :
            Inherit(*view->camera, xMin, yMin, xMax, yMax, initialArrayData),
            ECSVisitor(view->viewID)
        {
            //nop
        }

        bool intersectDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount) override
        {
            bool intersects = Inherit::intersectDraw(firstVertex, vertexCount, firstInstance, instanceCount);
            if (intersects && currentEntity != entt::null)
                collectedEntities.emplace_back(currentEntity);
            return intersects;
        }

        bool intersectDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount) override
        {
            bool intersects = Inherit::intersectDrawIndexed(firstIndex, indexCount, firstInstance, instanceCount);
            if (intersects && currentEntity != entt::null)
                collectedEntities.emplace_back(currentEntity);
            return intersects;
        }
    };

    /**
    * Specializes the VSG line segment intersector to locate entity components.
    */
    class ROCKY_EXPORT ECSLineSegmentIntersector : public vsg::Inherit<vsg::LineSegmentIntersector, ECSLineSegmentIntersector>,
        public ECSVisitor
    {
    public:
        ECSLineSegmentIntersector(vsg::View* view, int32_t x, int32_t y, vsg::ref_ptr<vsg::ArrayState> initialArrayData = {}) :
            Inherit(*view->camera, x, y, initialArrayData),
            ECSVisitor(view->viewID)
        {
            //nop
        }

        bool intersectDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount) override
        {
            bool intersects = Inherit::intersectDraw(firstVertex, vertexCount, firstInstance, instanceCount);
            if (intersects && currentEntity != entt::null)
                collectedEntities.emplace_back(currentEntity);
            return intersects;
        }

        bool intersectDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount) override
        {
            bool intersects = Inherit::intersectDrawIndexed(firstIndex, indexCount, firstInstance, instanceCount);
            if (intersects && currentEntity != entt::null)
                collectedEntities.emplace_back(currentEntity);
            return intersects;
        }
    };
}
