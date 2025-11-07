/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>
#include <memory>

namespace ROCKY_NAMESPACE
{
    // old type. upgrade these to ComponentBase2.
    struct ComponentBase
    {
        //! Revision, for synchronizing this component with another
        int revision = 0;

        //! Bump the revision. It's up to other systems to detect a change
        virtual void dirty()
        {
            ++revision;
        }

        entt::entity attach_point = entt::null;

        entt::entity owner = entt::null;
    };

    // Base component type with built-in dirty tracking.
    // NOTE: Yes, we need the CRTP here so that the "Dirty" object is unique for each derived type!
    template<class DERIVED>
    struct ComponentBase2
    {
        //! The entity that owns this component
        entt::entity owner = entt::null;
       
        // NOTE: RELIES on the System to install the Dirty singleton!
        struct Dirty
        {
            std::mutex mutex;
            std::vector<entt::entity> entities;
        };

        inline void dirty(entt::registry& r)
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(owner != entt::null, void());

            r.view<Dirty>().each([&](auto& dirtyList)
                {
                    std::scoped_lock lock(dirtyList.mutex);
                    dirtyList.entities.emplace_back(owner);
                });
        }

        template<class CALLABLE>
        inline static void eachDirty(entt::registry& r, CALLABLE&& func)
        {
            static_assert(std::is_invocable_v<CALLABLE, entt::entity>, "CALLABLE must be invocable with (entt::entity)");

            std::vector<entt::entity> entities;
            r.view<Dirty>().each([&](auto& dirtyList)
                {
                    std::scoped_lock lock(dirtyList.mutex);
                    entities.swap(dirtyList.entities);
                });

            for (auto e : entities)
            {
                // must check validity since it is possible the entity was destroyed
                // after being put on the dirty list.
                if (r.valid(e))
                {
                    func(e);
                }
            }
        }

        // Prevents an assignment from overwriting the component's owning entity.
        ComponentBase2& operator = (const ComponentBase2& rhs)
        {
            if (owner == entt::null)
                owner = rhs.owner;
            return *this;
        }
    };

    /**
     * ECS Component that holds a shared pointer to another component type.
     * This is useful for re-using a single basic component with different
     * transforms, visibility states, etc.
     *
     * WARNING -- EXPIERMENTAL -- API MAY CHANGE OR DISAPPEAR WITHOUT NOTICE!
     */
    template<typename T>
    struct Shareable : public ComponentBase
    {
        //! Shared pointer to the shared component
        std::shared_ptr<T> pointer;

        //! Construct a shared component. The pointer must be non-null
        //! at the time of construction and must stay non-null.
        [[deprecated("EXPERIMENTAL FEATURE")]]
        Shareable<T>(std::shared_ptr<T> p) : pointer(p) {
            ROCKY_HARD_ASSERT(p != nullptr);
        }
    };
}
