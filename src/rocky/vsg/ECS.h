/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/Transform.h>
#include <rocky/vsg/engine/Runtime.h>
#include <rocky/vsg/engine/Utils.h>
#include <vsg/vk/Context.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/Node.h>
#include <vector>
#include <chrono>
#include <type_traits>

//#define ENTT_NO_ETO
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    //! Entity Component System support
    namespace ECS
    {
        using time_point = std::chrono::steady_clock::time_point;

        /**
        * Superclass for ECS components meant to be revisioned and/or with an attach point.
        */
        struct RevisionedComponent
        {
            //! Revision, for synchronizing this component with another
            int revision = 0;
            void dirty() { revision++; }

            //! Attach point for additional components, as needed
            entt::entity entity = entt::null;

        protected:
            RevisionedComponent() = default;
        };

        /**
        * Component representing an entity's visibility.
        */
        struct Visibility
        {
            bool visible = true;
        };

        /**
        * Component that holds a VSG node and its revision (so it can be
        * synchronized with the associated data model). One will typically
        * attach a Renderable to RevisionedComponent::entity.
        */
        struct Renderable
        {
            vsg::ref_ptr<vsg::Node> node;
            int revision = -1;
        };

        /**
        * Base class for an ECS system. And ECS system is typically responsible
        * for performing logic around a specific type of component.
        */
        class ROCKY_EXPORT System
        {
        public:
            //! ECS entity registry
            entt::registry& registry;

            //! Status
            Status status;

            //! Initialize the ECS system (once at startup)
            virtual void initializeSystem(Runtime& runtime)
            {
                //nop
            }

            //! Update the ECS system (once per frame)
            virtual void update(Runtime& runtime)
            {
                updateComponents(runtime);
            }

        protected:
            System(entt::registry& in_registry) :
                registry(in_registry) { }

            //! Override this to handle any components that need initial setup
            virtual void updateComponents(Runtime& rutime)
            {
                //nop
            }
        };

        /**
        * VSG node representing an ECS system for the given component type (T).
        * A system of this type asumes that "T" will have a Renderable component attached
        * to "T::entity".
        * 
        * This lives under a SystemsManagerGroup node that will tick it each frame.
        *
        * @param T The component type
        */
        template<class T>
        class SystemNode :
            public vsg::Inherit<vsg::Compilable, SystemNode<T>>,
            public System
        {
            using super = vsg::Inherit<vsg::Compilable, SystemNode<T>>;

        public:
            //! Destructor
            virtual ~SystemNode<T>();

        protected:
            //! Construct from a subclass
            SystemNode<T>(entt::registry& in_registry);

            //! Called when the system detects a new or changed component.
            //! @param entity The entity to which the component is attached
            //! @param runtime Runtime context
            //! @return true if the update was successful; false if update should be
            //!   called again later.
            virtual bool update(entt::entity entity, Runtime& runtime) = 0;

            //! Feature mask for a specific component instance
            virtual int featureMask(const T& t) const { return 0; }

            // The configuration and command list for a graphics pipeline
            // configured for a specific set of features. This setup 
            // supports the creation of a unique pipeline for a feature set
            // that's stored in an integer mask.
            struct Pipeline
            {
                vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> config;
                vsg::ref_ptr<vsg::Commands> commands;
            };
            std::vector<Pipeline> pipelines;

            // Hooks to expose systems and components to VSG visitors.
            void compile(vsg::Context&) override;
            void traverse(vsg::Visitor& v) override;
            void traverse(vsg::ConstVisitor& v) const override;
            void traverse(vsg::RecordTraversal&) const override;

            //! Given a geospatial reference point, extract both an SRS op that will
            //! transform points from the component's SRS to the world SRS, and an offset
            //! for floating-point precision localization.
            //! @param point The reference point
            //! @param out_xform The SRS operation to transform points
            //! @param out_offset The offset to localize points
            bool setReferencePoint(const GeoPoint& point, SRSOperation& out_xform, vsg::dvec3& out_offset) const;

            //! Fetches the correct layout for a component.
            vsg::ref_ptr<vsg::PipelineLayout> getPipelineLayout(const T&) const;

        private:

            // list of entities whose components are out of date and need updating
            mutable std::vector<entt::entity> entities_to_update;

            // looks for any new components that need VSG initialization
            void updateComponents(Runtime&);

            // internal structure used when sorting components (by pipeline) for rendering
            struct RenderLeaf
            {
                Renderable& renderable;
                Transform* transform;
                bool visible;
            };

            // re-usable render set to prevent re-allocation
            // TODO: make sure this is multi-view/multi-thread safe; if not, put it in a
            // ViewLocal container
            mutable std::vector<std::vector<RenderLeaf>> renderSet;
        };

        /**
        * VSG Group node whose children are SystemNode instances. It can also hold/manager
        * non-node systems.
        */
        class ROCKY_EXPORT SystemsManagerGroup : public vsg::Inherit<vsg::Group, SystemsManagerGroup>
        {
        public:
            //! Add a system node instance to the group.
            //! @param system The system node instance to add
            template<class T>
            void add(vsg::ref_ptr<SystemNode<T>> system)
            {
                addChild(system);
                systems.emplace_back(system.get());
            }

            //! Create a system node of type T and add it to the group.
            //! @param entities The ECS entity registry to pass to the system node constructor
            //! @return The system node instance
            template<class T>
            vsg::ref_ptr<T> add(entt::registry& entities)
            {
                auto system = T::create(entities);
                if (system)
                {
                    addChild(system);
                    systems.emplace_back(system);
                }
                return system;
            }

            //! Add a non-node system to the group. This is a System instance that does not
            //! do any VSG stuff and is not part of the scene graph.
            //! @param system The non-node system to add
            void add(std::shared_ptr<System> system)
            {
                non_node_systems.emplace_back(system);
                systems.emplace_back(system.get());
            }

            //! Initialize of all connected system nodes. This should be invoked
            //! any time a new viewer is created.
            //! @param runtime The runtime object to pass to the systems
            void initialize(Runtime& runtime)
            {
                for (auto& system : systems)
                {
                    system->initializeSystem(runtime);
                }
            }

            //! Update all connected system nodes. This should be invoked once per frame.
            //! @param runtime The runtime object to pass to the systems
            void update(Runtime& runtime)
            {
                for (auto& system : systems)
                {
                    system->update(runtime);
                }
            }

        private:
            std::vector<System*> systems;
            std::vector<std::shared_ptr<System>> non_node_systems;
            // NB: SystmeNode instances are group children
        };


        class TransformSystem : public ECS::System
        {
        public:
            TransformSystem(entt::registry& r) : ECS::System(r) { }

            void update(Runtime& runtime) override
            {
                //auto view = registry.view<Transform::Dirty, Transform>();
                //for (auto&& [entity, xform] : view.each())
                //{
                //    xform.update();
                //}
                //registry.clear<Transform::Dirty>();
            }

            static std::shared_ptr<TransformSystem> create(entt::registry& r)
            {
                return std::make_shared<TransformSystem>(r);
            }
        };

        /**
        * Subclass of the EnTT registry providing additional utility functions.
        */
        class Registry : public entt::registry
        {
        public:
            //! Toggle the visibility of an entity
            void setVisible(entt::entity e, bool value)
            {
                get<ECS::Visibility>(e).visible = value;
            }

            //! Whether an entity is visible
            bool visible(entt::entity e)
            {
                return get<ECS::Visibility>(e).visible;
            }
        };
    }


    //........................................................................


    namespace detail
    {
        template<typename T>
        inline void SystemNode_on_construct(entt::registry& r, entt::entity e)
        {
            T& new_component = r.get<T>(e);

            // Add a visibility tag (if first time dealing with this entity)
            // I am not sure yet how to remove this in the end.
            if (!r.try_get<ECS::Visibility>(e))
            {
                r.emplace<ECS::Visibility>(e);
            }

            // Create a Renderable component and attach it to the new component.
            new_component.entity = r.create();
            r.emplace<ECS::Renderable>(new_component.entity);

            new_component.revision++;
        }

        template<typename T>
        inline void SystemNode_on_destroy(entt::registry& r, entt::entity e)
        {
            T& comp = r.get<T>(e);
            r.remove<ECS::Renderable>(comp.entity);

            // Only want to remove it if there are no more components on this entity. I guess.
            //r.remove<ECS::Visibility>(e);
        }
    }

    template<class T>
    ECS::SystemNode<T>::SystemNode(entt::registry& in_registry) :
        System(in_registry)
    {
        registry.on_construct<T>().template connect<&detail::SystemNode_on_construct<T>>();
        registry.on_destroy<T>().template connect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    ECS::SystemNode<T>::~SystemNode()
    {
        registry.on_construct<T>().template disconnect<&detail::SystemNode_on_construct<T>>();
        registry.on_destroy<T>().template disconnect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    inline void ECS::SystemNode<T>::traverse(vsg::Visitor& v)
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.entity);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        super::traverse(v);
    }

    //! Pass-thru for VSG const visitors
    template<class T>
    inline void ECS::SystemNode<T>::traverse(vsg::ConstVisitor& v) const
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.entity);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        super::traverse(v);
    }

    template<class T>
    inline void ECS::SystemNode<T>::compile(vsg::Context& context)
    {
        // Compile the pipelines
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->compile(context);
        }

        // Compile the components
        util::SimpleCompiler compiler(context);

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.entity);
                if (renderable.node)
                    renderable.node->accept(compiler);
            });
    }

    template<class T>
    inline void ECS::SystemNode<T>::traverse(vsg::RecordTraversal& rt) const
    {
        const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

        // Sort components into render sets by pipeline. If this system doesn't support
        // multiple pipelines, just store them all together in renderSet[0].
        if (renderSet.empty())
            renderSet.resize(!pipelines.empty() ? pipelines.size() : 1);

        // Get an optimized view of all this system's components:
        registry.view<T, Visibility>().each([&](const entt::entity entity, const T& component, auto& visibility)
            {
                auto& renderable = registry.get<Renderable>(component.entity);
                if (renderable.node)
                {
                    auto& rs = !pipelines.empty() ? renderSet[featureMask(component)] : renderSet[0];
                    auto* transform = registry.try_get<Transform>(entity);
                    if (transform || visibility.visible)
                        rs.emplace_back(RenderLeaf{ renderable, transform, visibility.visible });
                }

                if (!renderable.node || (renderable.revision != component.revision))
                {
                    entities_to_update.push_back(entity);
                }
            });

        // Time to record all visible components.
        // For each pipeline:
        for (int p = 0; p < renderSet.size(); ++p)
        {
            if (!renderSet[p].empty())
            {
                // Bind the Graphics Pipeline for this render set, if there is one:
                if (!pipelines.empty())
                {
                    pipelines[p].commands->accept(rt);
                }

                // Them record each component. If the component has a transform apply it too.
                for (auto& leaf : renderSet[p])
                {
                    if (leaf.transform)
                    {
                        if (leaf.transform->push(rt, identity_matrix))
                        {
                            if (leaf.visible)
                                leaf.renderable.node->accept(rt);

                            leaf.transform->pop(rt);
                        }
                    }
                    else
                    {
                        if (leaf.visible)
                            leaf.renderable.node->accept(rt);
                    }
                }

                // clear out for next time around.
                renderSet[p].clear();
            }
        }
    }


    template<class T>
    inline void ECS::SystemNode<T>::updateComponents(Runtime& runtime)
    {
        for (auto& entity : entities_to_update)
        {
            if (update(entity, runtime))
            {
                // TODO: just do this in the update call, since the subclass is probably
                // already looking up the renderable?
                auto& comp = registry.get<T>(entity);
                auto& renderable = registry.get<Renderable>(comp.entity);
                renderable.revision = comp.revision;
            }
        }

        entities_to_update.clear();
    }

    template<class T>
    bool ECS::SystemNode<T>::setReferencePoint(const GeoPoint& point, SRSOperation& out_xform, vsg::dvec3& out_offset) const
    {
        if (point.srs.valid())
        {
            SRS worldSRS = point.srs;

            if (point.srs.isGeodetic())
            {
                worldSRS = point.srs.geocentricSRS();
                GeoPoint world = point.transform(worldSRS);
                if (world.valid())
                {
                    out_offset = vsg::dvec3{ world.x, world.y, world.z };
                }
            }
            else
            {
                out_offset = vsg::dvec3{ point.x, point.y, point.z };
            }

            out_xform = SRSOperation(point.srs, worldSRS);
            return true;
        }
        return false;
    }

    template<class T>
    vsg::ref_ptr<vsg::PipelineLayout> ECS::SystemNode<T>::getPipelineLayout(const T& t) const
    {
        return pipelines.empty() ? nullptr : pipelines[featureMask(t)].config->layout;
    }
}
