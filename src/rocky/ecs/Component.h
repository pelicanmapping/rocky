/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>
#include <cstdint>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        //! Process-wide component generation source. This must live in the
        //! Rocky library (rather than a template static) so DLL clients and
        //! the library cannot generate duplicate revisions.
        extern ROCKY_EXPORT std::uint64_t nextComponentRevision();
    }

    // Base component type with built-in dirty tracking.
    // NOTE: Yes, we need the CRTP here so that the "Dirty" object is unique for each derived type!
    template<class DERIVED>
    struct Component
    {
        //! The entity that owns this component
        entt::entity owner = entt::null;

        // NOTE: RELIES on the System to install the Dirty singleton!
        // NOTE: type of this struct is Component<DERIVED>::Dirty
        struct Dirty
        {
            std::mutex mutex;
            std::vector<entt::entity> entities;
        };

        //! Persistent generation assigned whenever this component is dirtied.
        //! Unlike the consumable Dirty queue, this remains available to any
        //! number of downstream systems for non-destructive change detection.
        std::uint64_t componentRevision() const noexcept
        {
            return _componentRevision;
        }

        //! Add this component to a global "dirty" collection.
        //! A system is responsible for installing the Dirty singleton by calling
        //! registry.emplace<MyComponent:Dirty>(registry.create());
        inline void dirty(entt::registry& r)
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(owner != entt::null, void(),
                "ComponentBase2::dirty() called on unowned component - on_construct() was probably not installed for this type; "
                "you might need to call Application::realize() before creating ECS components");

            // Use a process-wide monotonic generation instead of incrementing the
            // stored value. This guarantees that emplace_or_replace() remains
            // observable even when the incoming component was copied from an
            // object with an unrelated revision.
            _componentRevision = detail::nextComponentRevision();

            r.view<Dirty>().each([&](auto& dirtyList)
                {
                    std::scoped_lock lock(dirtyList.mutex);
                    dirtyList.entities.emplace_back(owner);
                });
        }

        //! Add this component to a global "dirty" collection.
        //! A system is responsible for installing the Dirty singleton by calling
        //! registry.emplace<MyComponent:Dirty>(registry.create());
        inline static void dirty(entt::registry& r, entt::entity e)
        {
            r.get<DERIVED>(e).owner = e;
            r.get<DERIVED>(e).dirty(r);
        }

        //! Iterate over all dirty components of this type, invoking the given callable
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

    private:
        std::uint64_t _componentRevision = 0u;
    };
}
