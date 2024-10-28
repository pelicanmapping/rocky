/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/GeoTransform.h>
#include <rocky/vsg/engine/Runtime.h>
#include <rocky/vsg/engine/Utils.h>
#include <vsg/vk/Context.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/Node.h>
#include <entt/entt.hpp>
#include <vector>
#include <chrono>
#include <type_traits>

namespace ROCKY_NAMESPACE
{
    //! Entity Component System support
    namespace ECS
    {
        using time_point = std::chrono::steady_clock::time_point;

        /**
        * Base class for all ECS components.
        */
        class ROCKY_EXPORT Component
        {
        public:
            //! Component readable name
            std::string name;

            //! Whether to use this component
            bool active = true;

            //! Component developers can use this to tie this component's
            //! visiblity to another component.
            bool* active_ptr = &active;

            //! Revision number. Bump this to dirty the component.
            int revision = 0;

            //! Serialize this component to a JSON string
            virtual std::string to_json() const;
            
            //! Bump the revision number to indicate that the component has changed
            inline void dirty() { ++revision; }
        };

        /**
        * Base class for a ECS Component that manifests itself as an VSG node.
        */
        class ROCKY_EXPORT NodeComponent : public Component
        {
        public:
            //! VSG node that renders this component
            vsg::ref_ptr<vsg::Node> node;

            //! Whether to reinitialize this component's node
            bool nodeDirty = true;
        };

        /**
        * Base class for an ECS system.
        */
        class ROCKY_EXPORT System
        {
        public:
            //! ECS entity registry
            entt::registry& registry;

            //! Status
            Status status;

            //! Initialize the ECS system (once at startup)
            virtual void initializeSystem(Runtime& runtime) { }

            //! Update the ECS system (once per frame)
            virtual void update(Runtime& runtime)
            {
                updateComponents(runtime);
            }

        protected:
            System(entt::registry& in_registry) :
                registry(in_registry) { }

            //! Override this to handle any components that need initial setup
            virtual void updateComponents(Runtime& rutime) { }
        };

        /**
        * VSG node representing an ECS system for the given component type (T) and corresponding
        * renderable type (R). This lives under a SystemsManagerGroup node.
        * @param T The component type
        * @param R The renderable type (a struct that contains drawable data if necessary)
        */
        template<class T, class R>
        class SystemNode :
            public vsg::Inherit<vsg::Compilable, SystemNode<T, R>>,
            public System
        {
        public:
            struct InitContext
            {
                vsg::ref_ptr<vsg::PipelineLayout> layout;
                vsg::ref_ptr<vsg::Options> readerWriterOptions;
                vsg::ref_ptr<vsg::SharedObjects> sharedObjects;
            };

        protected:
            //! Construct from a subclass
            SystemNode<T, R>(entt::registry& in_registry) :
                Inherit(), System(in_registry) { }

            //! Called when the system detects a new component.
            //! @param entity The entity that has the new component
            //! @param context The context for initializing the component
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
            void accept(vsg::Visitor& v) override;
            void accept(vsg::ConstVisitor& v) const override;
            void compile(vsg::Context&) override;
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
        };

        /**
        * VSG Group node whose children are VSG_System objects. It can also hold/manager
        * non-node systems.
        */
        class SystemsManagerGroup : public vsg::Inherit<vsg::Group, SystemsManagerGroup>
        {
        public:
            //! Add a system node instance to the group.
            //! @param system The system node instance to add
            template<class T, class R>
            void add(vsg::ref_ptr<SystemNode<T,R>> system)
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

            //! Add a non-node system to the group.
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
            // node: node systems are group children.
        };
    }

    /**
    * ECS Component that provides an entity with a geotransform.
    */
    struct ROCKY_EXPORT Transform : public ECS::Component
    {
        vsg::ref_ptr<GeoTransform> node;
        vsg::dmat4 local_matrix = vsg::dmat4(1.0);
        Transform* parent = nullptr;

        //! Sets the transform's geoposition, creating the node on demand
        void setPosition(const GeoPoint& p)
        {
            if (!node)
                node = GeoTransform::create();

            node->setPosition(p);
        }

        //! Returns true if the push succeeded (and a pop will be required)
        inline bool push(vsg::RecordTraversal& rt, const vsg::dmat4& m)
        {
            if (node)
            {
                return node->push(rt, m * local_matrix);
            }
            else if (parent)
            {
                return parent->push(rt, m * local_matrix);
            }
            else return false;
        }

        inline void pop(vsg::RecordTraversal& rt)
        {
            if (node)
            {
                node->pop(rt);
            }
            else if (parent)
            {
                parent->pop(rt);
            }
        }
    };

    /**
    * ECS Component representing a moving entity
    */
    struct Motion : public ECS::Component
    {
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
    };

    /**
    * ECS System to process Motion components
    */
    class ROCKY_EXPORT EntityMotionSystem : public ECS::System
    {
    public:
        EntityMotionSystem(entt::registry& r) : ECS::System(r) { }

        //! Called to update the transforms
        void update(Runtime& runtime) override;

    private:
        //! Constructor
        ECS::time_point last_time = ECS::time_point::min();
    };


    template<class T, class R>
    inline void ECS::SystemNode<T,R>::accept(vsg::Visitor& v)
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<R>().each([&](const auto e, auto& renderable)
            {
                if (renderable.node)
                    renderable.node->accept(v);
            });
    }

    //! Pass-thru for VSG const visitors
    template<class T, class R>
    inline void ECS::SystemNode<T,R>::accept(vsg::ConstVisitor& v) const
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<R>().each([&](const auto e, auto& renderable)
            {
                if (renderable.node)
                    renderable.node->accept(v);
            });
    }

    template<class T, class R>
    inline void ECS::SystemNode<T, R>::compile(vsg::Context& context)
    {
        // Compile the pipelines
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->compile(context);
        }

        // Compile the components
        util::SimpleCompiler compiler(context);
        auto view = registry.view<R>();
        view.each([&](const auto entity, auto& renderable)
            {
                renderable.node->accept(compiler);
            });
    }

    template<class T, class R>
    inline void ECS::SystemNode<T,R>::traverse(vsg::RecordTraversal& rt) const
    {
        const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

        using Entry = struct
        {
            const R& renderable;
            entt::entity entity;
        };

        // Sort components into render sets by pipeline.
        // If this system doesn't support multiple pipelines, just 
        // store them all together
        std::vector<std::vector<Entry>> render_set(!pipelines.empty() ? pipelines.size() : 1);

        // Get an optimized view of all this system's components:
        registry.view<T, R>().each([&](const entt::entity entity, const T& component, const R& renderable)
            {
                // Is the component visible?
                if (*component.active_ptr)
                {
                    // Does it have a VSG node? If so, queue it up under the
                    // appropriate pipeline.
                    if (renderable.node)
                    {
                        auto& rs = !pipelines.empty() ? render_set[featureMask(component)] : render_set[0];
                        rs.emplace_back(Entry{ renderable, entity });
                    }

                    if (!renderable.node || (renderable.revision != component.revision))
                    {
                        entities_to_update.push_back(entity);
                    }
                }
            });

        // Time to record all visible components.
        // For each pipeline:
        for (int p = 0; p < render_set.size(); ++p)
        {
            if (!render_set[p].empty())
            {
                // Bind the Graphics Pipeline for this render set, if there is one:
                if (!pipelines.empty())
                {
                    pipelines[p].commands->accept(rt);
                }

                // Them record each component.
                // If the component has a transform apply it too.
                for (auto& e : render_set[p])
                {
                    auto* xform = registry.try_get<Transform>(e.entity);
                    if (xform)
                    {
                        if (xform->push(rt, identity_matrix))
                        {
                            e.renderable.node->accept(rt);
                            xform->pop(rt);
                        }
                    }
                    else
                    {
                        e.renderable.node->accept(rt);
                    }
                }
            }
        }
    }


    template<class T, class R>
    inline void ECS::SystemNode<T, R>::updateComponents(Runtime& runtime)
    {
        for (auto& entity : entities_to_update)
        {
            if (update(entity, runtime))
            {
                auto& [component, renderable] = registry.get<T, R>(entity);
                renderable.revision = component.revision;
            }
        }

        entities_to_update.clear();
    }

    template<class T, class R>
    bool ECS::SystemNode<T,R>::setReferencePoint(const GeoPoint& point, SRSOperation& out_xform, vsg::dvec3& out_offset) const
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

    template<class T, class R>
    vsg::ref_ptr<vsg::PipelineLayout> ECS::SystemNode<T,R>::getPipelineLayout(const T& t) const
    {
        return pipelines.empty() ? nullptr : pipelines[featureMask(t)].config->layout;
    }
}
