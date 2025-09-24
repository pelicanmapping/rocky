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
    struct BaseComponent
    {
        //! Revision, for synchronizing this component with another
        int revision = 0;

        //! Bump the revision. It's up to other systems to detect a change
        virtual void dirty()
        {
            ++revision;
        }

        //! Constructors
        BaseComponent() = default;
        BaseComponent(const BaseComponent&) = default;
        BaseComponent& operator = (const BaseComponent&) = default;
        BaseComponent(BaseComponent&& rhs) noexcept {
            *this = rhs;
        }
        BaseComponent& operator = (BaseComponent&& rhs) noexcept {
            if (this != &rhs) {
                revision = rhs.revision;
                rhs.revision = 0;
            }
            return *this;
        }
    };


    /**
    * Superclass for ECS components that support revisionings and an attach point.
    * The attach point is for internal usage.
    */
    struct AttachableComponent : public BaseComponent
    {
        //! Attach point for additional components, as needed
        entt::entity attach_point = entt::null;

        //! Constructors
        AttachableComponent() = default;
        AttachableComponent(const AttachableComponent&) = default;
        AttachableComponent& operator = (const AttachableComponent&) = default;
        AttachableComponent(AttachableComponent&& rhs) noexcept {
            *this = rhs;
        }
        AttachableComponent& operator = (AttachableComponent&& rhs) noexcept {
            if (this != &rhs) {
                BaseComponent::operator = (rhs);
                attach_point = rhs.attach_point;
                rhs.attach_point = entt::null;
            }
            return *this;
        }
    };

    /**
     * ECS Component that holds a shared pointer to another component type.
     * This is useful for re-using a single basic component with different
     * transforms, visibility states, etc.
     */
    template<typename T>
    struct SharedComponent : public BaseComponent
    {
        //! Shared pointer to the shared component
        std::shared_ptr<T> pointer;

        //! Construct a shared component. The pointer must be non-null
        //! at the time of construction and must stay non-null.
        SharedComponent<T>(std::shared_ptr<T> p) : pointer(p) {
            ROCKY_HARD_ASSERT(p != nullptr);
        }
    };
}
