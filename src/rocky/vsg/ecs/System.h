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
    /**
    * Base class for an ECS system. And ECS system is typically responsible
    * for performing logic around a specific type of component.
    */
    class ROCKY_EXPORT System
    {
    public:
        //! Status
        Result<> status;

        //! Initialize the ECS system (once at startup)
        virtual void initialize(VSGContext& runtime)
        {
            //nop
        }

        //! Update the ECS system (once per frame)
        virtual void update(VSGContext& runtime)
        {
            //nop
        }

    protected:
        System(Registry in_registry) :
            _registry(in_registry) {
        }

        //! ECS entity registry
        Registry _registry;
    };
}
