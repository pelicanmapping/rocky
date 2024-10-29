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

        struct VisibleComponent
        {
            //! Visibility
            bool visible = true;
            bool* visible_ptr = &visible;

            //! Version
            int revision = 0;
            void dirty() { revision++; }

            entt::entity entity = entt::null;

        protected:
            VisibleComponent() = default;
        };

        struct Renderable
        {
            vsg::ref_ptr<vsg::Node> node;
            int revision = 0;
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
        * @param R The renderdata type (a struct that contains drawable data if necessary)
        */
        template<class T>
        class SystemNode :
            public vsg::Inherit<vsg::Compilable, SystemNode<T>>,
            public System
        {
        public:
            struct InitContext
            {
                vsg::ref_ptr<vsg::PipelineLayout> layout;
                vsg::ref_ptr<vsg::Options> readerWriterOptions;
                vsg::ref_ptr<vsg::SharedObjects> sharedObjects;
            };

            //! Destructor
            virtual ~SystemNode<T>();

        protected:
            //! Construct from a subclass
            SystemNode<T>(entt::registry& in_registry);

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

            struct RenderLeaf
            {
                Renderable& renderable;
                entt::entity entity;
            };
            mutable std::vector<std::vector<RenderLeaf>> renderSet;

        public:
            //! Callbacks for component lifecycle
            void onConstruct(entt::registry& r, const entt::entity);
            void onDestroy(entt::registry& r, const entt::entity);
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
    struct Transform
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
    struct Motion : public ECS::VisibleComponent
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


    //........................................................................


    template<class T>
    ECS::SystemNode<T>::SystemNode(entt::registry& in_registry) :
        Inherit(), System(in_registry)
    {
        registry.on_construct<T>().connect<&SystemNode<T>::onConstruct>(*this);
        registry.on_destroy<T>().connect<&SystemNode<T>::onDestroy>(*this);
    }

    template<class T>
    ECS::SystemNode<T>::~SystemNode()
    {
        registry.on_construct<T>().disconnect<&SystemNode<T>::onConstruct>(*this);
        registry.on_destroy<T>().disconnect<&SystemNode<T>::onDestroy>(*this);
    }

    template<class T>
    void ECS::SystemNode<T>::onConstruct(entt::registry& r, const entt::entity e)
    {
        //r.emplace<R>(e);

        T& comp = r.get<T>(e);
        comp.entity = r.create();
        r.emplace<Renderable>(comp.entity);
    }

    template<class T>
    void ECS::SystemNode<T>::onDestroy(entt::registry& r, const entt::entity e)
    {
        //r.remove<R>(e);
    }

    template<class T>
    inline void ECS::SystemNode<T>::traverse(vsg::Visitor& v)
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](const auto e, auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.entity);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        Inherit::traverse(v);
    }

    //! Pass-thru for VSG const visitors
    template<class T>
    inline void ECS::SystemNode<T>::traverse(vsg::ConstVisitor& v) const
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](const auto e, auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.entity);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        Inherit::traverse(v);
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

        registry.view<T>().each([&](const auto e, auto& c)
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

        //struct Entry
        //{
        //    Renderable& renderable;
        //    entt::entity entity;
        //};

        // Sort components into render sets by pipeline. If this system doesn't support
        // multiple pipelines, just store them all together
        // TODO: see if this is multi-view safe
        if (renderSet.empty())
            renderSet.resize(!pipelines.empty() ? pipelines.size() : 1);

        // Get an optimized view of all this system's components:
        registry.view<T>().each([&](const entt::entity entity, const T& component)
            {
                // Is the component visible?
                if (*component.visible_ptr)
                {
                    auto& renderable = registry.get<Renderable>(component.entity);
                    if (renderable.node)
                    {
                        auto& rs = !pipelines.empty() ? renderSet[featureMask(component)] : renderSet[0];
                        rs.emplace_back(RenderLeaf{ renderable, entity });
                    }

                    if (!renderable.node || (renderable.revision != component.revision))
                    {
                        entities_to_update.push_back(entity);
                    }
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

                // Them record each component.
                // If the component has a transform apply it too.
                for (auto& leaf : renderSet[p])
                {
                    auto* xform = registry.try_get<Transform>(leaf.entity);
                    if (xform)
                    {
                        if (xform->push(rt, identity_matrix))
                        {
                            leaf.renderable.node->accept(rt);
                            xform->pop(rt);
                        }
                    }
                    else
                    {
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
