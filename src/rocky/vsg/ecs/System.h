/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    class RenderTextureParticipant;

    /**
    * Base class for an ECS system. And ECS system is typically responsible
    * for performing logic around a specific type of component.
    */
    class ROCKY_EXPORT System
    {
    public:
        //! Status
        Status status;

        //! Initialize the ECS system (once at startup)
        virtual void initialize(VSGContext vsgcontext)
        {
            //nop
        }

        //! Update the ECS system (once per frame)
        virtual void update(VSGContext vsgcontext)
        {
            //nop
        }

        /**
         * Return the node that participates in generic render-to-texture passes,
         * or null when this system has no record traversal for them.
         *
         * This capability removes the central list of geometry system types;
         * an extension opts in without modifying Application or Overlay code.
         */
        virtual RenderTextureParticipant* renderTextureParticipant()
        {
            return nullptr;
        }

    protected:
        System(Registry in_registry) :
            _registry(in_registry) {
        }

        //! ECS entity registry
        Registry _registry;
    };
}
