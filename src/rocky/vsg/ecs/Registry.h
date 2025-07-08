/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <chrono>
#include <type_traits>
#include <shared_mutex>
#include <mutex>
#include <utility> // std::pair

#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    //! Entity Component System support
    namespace ecs
    {
        //! Alias for time_point
        using time_point = std::chrono::steady_clock::time_point;

        /**
        * Wraps the ECS registry with a read-write lock for thread safety.
        *
        * Take an exclusive (write) lock when calling entt::registry methods that
        * alter the database, like:
        *   - create, destroy, emplace, remove
        *
        * Take a shared (read) lock when calling entt::registry methods like:
        *   - get, view
        *   - and when updating components in place
        */
        class Registry
        {
        public:
            struct Read {
                std::shared_lock<std::shared_mutex> lock;
                entt::registry& registry;
                entt::registry* operator->() { return &registry; }
                operator std::pair<std::shared_lock<std::shared_mutex>, entt::registry&>() { return { std::move(lock), registry }; }
            };
            struct Write {
                std::unique_lock<std::shared_mutex> lock;
                entt::registry& registry;
                entt::registry* operator->() { return &registry; }
                operator std::pair<std::unique_lock<std::shared_mutex>, entt::registry&>() { return { std::move(lock), registry }; }
            };

            //! Returns a reference to a read-locked EnTT registry.
            //! 
            //! A read-lock is appropriate for get(), view(), and in-place updates to existing
            //! components. The read-lock is scoped and will automatically release at the 
            //! closing of the usage scope.
            //! 
            //! usage:
            //!   auto [lock, registry] = ecs_registry.read();
            //! 
            //! @return A tuple including a scoped shared lock and a reference to the underlying registry
            Read read() const {
                return { std::shared_lock(_impl->_mutex), _impl->_registry };
            }
            Read update() const {
                return read();
            }

            //! Returns a reference to a write-locked EnTT registry.
            //! 
            //! A write-lock is appropritae for calls to create(), destroy(), clear(), emplace().
            //! Note: you do not need a write lock for in-place component changes.
            //! 
            //! usage:
            //!   auto [lock, registry] = ecs_registry.write();
            //! 
            //! @return A tuple including a scoped unique lock and a reference to the underlying registry
            Write write() const {
                return { std::unique_lock(_impl->_mutex), _impl->_registry };
            }

            //! Convenience function to invoke a lambda with a read-locked registry reference.
            //! The signature of CALLABLE must match void(entt::registry&).
            template<typename CALLABLE>
            void read(CALLABLE&& func) const {
                static_assert(std::is_invocable_r_v<void, CALLABLE, entt::registry&>, "Callable must match void(entt::registry&)");
                auto [lock, registry] = read();
                func(registry);
            }

            template<typename CALLABLE>
            void update(CALLABLE&& func) const {
                static_assert(std::is_invocable_r_v<void, CALLABLE, entt::registry&>, "Callable must match void(entt::registry&)");
                auto [lock, registry] = update();
                func(registry);
            }

            //! Convenience function to invoke a lambda with a write-locked registry reference.
            //! The signature of CALLABLE must match void(entt::registry&).
            template<typename CALLABLE>
            void write(CALLABLE&& func) const {
                static_assert(std::is_invocable_r_v<void, CALLABLE, entt::registry&>, "Callable must match void(entt::registry&)");
                auto [lock, registry] = write();
                func(registry);
            }

            //! Default constructor - empty registry
            Registry() = default;

            //! Copy constructor
            Registry(const Registry& rhs) = default;

            //! Creator
            static Registry create() {
                Registry r;
                r._impl = std::make_shared<Impl>();
                return r;
            }

        private:
            struct Impl {
                mutable std::shared_mutex _mutex;
                mutable entt::registry _registry;
            };
            std::shared_ptr<Impl> _impl;
        };

        /**
        * Base class for an ECS system. And ECS system is typically responsible
        * for performing logic around a specific type of component.
        */
        class ROCKY_EXPORT System
        {
        public:
            //! ECS entity registry
            Registry _registry;

            //! Status
            Status status;

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
            System(Registry& in_registry) :
                _registry(in_registry) { }
        };
    }
}
