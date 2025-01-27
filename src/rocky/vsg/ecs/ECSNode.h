/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/Registry.h>
#include <rocky/vsg/ecs/Component.h>
#include <rocky/vsg/ecs/Visibility.h>
#include <rocky/vsg/ecs/Transform.h>
#include <rocky/vsg/Utils.h>
#include <rocky/Utils.h>
#include <vsg/vk/Context.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/Node.h>
#include <thread>

namespace ROCKY_NAMESPACE
{
    namespace ecs
    {
        class SystemNodeBase;

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

        //! Information passed to a system when creating or updating a node
        struct BuildInfo
        {
            vsg::ref_ptr<vsg::Node> existing_node;
            vsg::ref_ptr<vsg::Node> new_node;
        };

        // Internal record for a component that needs building
        struct BuildItem : public BuildInfo
        {
            entt::entity entity = entt::null;
            std::uint16_t version = 0;
            RevisionedComponent* component = nullptr;
            ~BuildItem() { if (component) delete component; }

            // only permit default or move construction:
            BuildItem() = default;
            BuildItem(const BuildItem&) = delete;
            BuildItem& operator=(const BuildItem& rhs) = delete;
            BuildItem(BuildItem&& rhs) noexcept { *this = std::move(rhs); }
            BuildItem& operator = (BuildItem&& rhs) noexcept {
                entity = rhs.entity;
                version = rhs.version;
                existing_node = rhs.existing_node;
                new_node = rhs.new_node;
                component = rhs.component;
                rhs.component = nullptr;
                return *this;
            }
        };

        // Internal structure for a batch of BuildItems associated with a system
        struct BuildBatch
        {
            std::vector<BuildItem> items;
            vsg::ref_ptr<SystemNodeBase> system;
            VSGContext* context = nullptr;

            // only permit default or move construction:
            BuildBatch() = default;
            BuildBatch(BuildBatch&& rhs) noexcept = default;
            BuildBatch& operator=(BuildBatch&& rhs) noexcept = default;
            BuildBatch(const BuildBatch&) = delete;
            BuildBatch& operator=(const BuildBatch& rhs) = delete;
        };

        // Internal utility for compiling nodes in a background thread
        class EntityNodeFactory
        {
        public:
            //! Start the compiler's thread
            void start();

            //! Stop the compiler's thread
            void quit();

            //! Called during update, this will merge any compilation results into the scene
            void mergeResults(Registry&, VSGContext&);

            // the data structures holding the queued jobs. 16 might be overkill :)
            struct Buffers
            {
                util::ring_buffer_with_condition<BuildBatch> input{ 16 };
                util::ring_buffer<BuildBatch> output{ 16 };
            };

            std::shared_ptr<Buffers> buffers = nullptr;
            std::thread thread;
        };

        /**
        * VSG node representing an ECS system for the given component type (T).
        * A system of this type asumes that "T" will have a Renderable component attached
        * to "T::entity".
        *
        * This lives under a ECSNode node that will tick it each frame.
        *
        * @param T The component type
        */
        class SystemNodeBase : public vsg::Inherit<vsg::Compilable, SystemNodeBase>
        {
        public:
            EntityNodeFactory* factory = nullptr;

            virtual void invokeCreateOrUpdate(BuildItem& item, VSGContext& runtime) const = 0;

            virtual void mergeCreateOrUpdateResults(entt::registry&, BuildItem& item, VSGContext& runtime) = 0;
        };

        template<class T>
        class SystemNode : public vsg::Inherit<SystemNodeBase, SystemNode<T>>, public System
        {
            using super = vsg::Inherit<SystemNodeBase, SystemNode<T>>;

        public:
            //! Destructor
            virtual ~SystemNode();

            // looks for any new components that need VSG initialization
            void update(VSGContext&) override;

        protected:
            //! Construct from a subclass
            SystemNode(Registry& in_registry);

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
            bool parseReferencePoint(const GeoPoint& point, SRSOperation& out_xform, vsg::dvec3& out_offset) const;

            //! Fetches the correct layout for a component.
            vsg::ref_ptr<vsg::PipelineLayout> getPipelineLayout(const T&) const;

            //! Subclass must implement this to create or update a node for a component.
            virtual void createOrUpdateNode(T&, BuildInfo&, VSGContext&) const = 0;

            void invokeCreateOrUpdate(BuildItem& item, VSGContext& runtime) const override;

            void mergeCreateOrUpdateResults(entt::registry& registry, BuildItem& item, VSGContext& runtime) override;

        private:

            // list of entities whose components are out of date and need updating
            mutable std::vector<entt::entity> entities_to_update;

            // internal structure used when sorting components (by pipeline) for rendering
            struct RenderLeaf
            {
                Renderable& renderable;
                Transform* transform;
            };

            // re-usable collection to minimize re-allocation
            mutable std::vector<std::vector<RenderLeaf>> pipelineRenderLeaves;
        };

        /**
        * VSG Group node whose children are SystemNode instances. It can also hold/manager
        * non-node systems. It also holds the container for pending entity compilation lists.
        */
        class ROCKY_EXPORT ECSNode : public vsg::Inherit<vsg::Group, ECSNode>
        {
        public:
            //! Construct
            //! @param reg The entity registry
            ECSNode(Registry& reg);

            // Destruct
            ~ECSNode();

            //! Add a system node instance to the group.
            //! @param system The system node instance to add
            template<class T>
            void add(vsg::ref_ptr<T> system)
            {
                static_assert(std::is_base_of<SystemNodeBase, T>::value, "T must be a subclass of SystemNodeBase");
                addChild(system);
                systems.emplace_back(system.get());
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
            void initialize(VSGContext& runtime)
            {
                for (auto& child : children)
                {
                    auto systemNode = child->cast<SystemNodeBase>();
                    if (systemNode)
                    {
                        systemNode->factory = &factory;
                    }
                }

                for (auto& system : systems)
                {
                    system->initializeSystem(runtime);
                }
            }

            //! Update all connected system nodes. This should be invoked once per frame.
            //! @param runtime The runtime object to pass to the systems
            void update(VSGContext& runtime);

        
            std::vector<System*> systems;
            std::vector<std::shared_ptr<System>> non_node_systems;
            Registry& registry;
            EntityNodeFactory factory;
        };

        //! Whether the Visibility component is visible in the given view
        //! @param vis The visibility component
        //! @param view_index Index of view to check visibility
        //! @return True if visible in that view
        inline bool visible(const Visibility& vis, int view_index)
        {
            return vis.parent != nullptr ? visible(*vis.parent, view_index) : vis[view_index]; // (vis.active && vis[view_index]);
        }

        //! Toggle the visibility of an entity in the given view
        //! @param r Entity registry
        //! @param e Entity id
        //! @param value New visibility state
        //! @param view_index Index of view to set visibility
        inline void setVisible(entt::registry& registry, entt::entity e, bool value, int view_index = -1)
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(e != entt::null, void());

            auto& visibility = registry.get<Visibility>(e);
            if (visibility.parent == nullptr)
            {
                if (view_index >= 0)
                    visibility[view_index] = value;
                else
                    visibility.fill(value);
            }
        }

        //! Whether an entity is visible in the given view
        //! @param r Entity registry
        //! @param e Entity id
        //! @param view_index Index of view to check visibility
        //! @return True if visible in that view
        inline bool visible(entt::registry& r, entt::entity e, int view_index = 0)
        {
            // assume a readlock on the registry
            ROCKY_SOFT_ASSERT_AND_RETURN(e != entt::null, false);

            return visible(r.get<Visibility>(e), view_index);
        }
    }


    //........................................................................


    namespace ecs
    {
        namespace detail
        {
            // called by registry.emplace<T>()
            template<typename T>
            inline void SystemNode_on_construct(entt::registry& r, entt::entity e)
            {
                T& new_component = r.get<T>(e);

                if (!r.try_get<ActiveState>(e))
                {
                    r.emplace<ActiveState>(e);
                }

                // Add a visibility tag (if first time dealing with this component)
                // I am not sure yet how to remove this in the end.
                if (!r.try_get<Visibility>(e))
                {
                    r.emplace<Visibility>(e);
                }

                // Create a Renderable component and attach it to the new component.
                new_component.attach_point = r.create();
                r.emplace<ecs::Renderable>(new_component.attach_point);

                new_component.revision++;
            }

            // invoked by registry.replace<T>(), emplace_or_replace<T>(), or patch<T>()
            template<typename T>
            inline void SystemNode_on_update(entt::registry& r, entt::entity e)
            {
                T& updated_component = r.get<T>(e);

                if (updated_component.attach_point == entt::null)
                {
                    updated_component.attach_point = r.create();
                    r.emplace<ecs::Renderable>(updated_component.attach_point);
                }

                updated_component.revision++;
            }

            // invoked by registry.erase<T>(), remove<T>(), or registry.destroy(e)
            template<typename T>
            inline void SystemNode_on_destroy(entt::registry& r, entt::entity e)
            {
                T& component_being_destroyed = r.get<T>(e);
                r.destroy(component_being_destroyed.attach_point);
            }
        }
    }

    template<class T>
    ecs::SystemNode<T>::SystemNode(Registry& in_registry) :
        System(in_registry)
    {
        auto [lock, registry] = _registry.write();

        registry.on_construct<T>().template connect<&detail::SystemNode_on_construct<T>>();
        registry.on_update<T>().template connect<&detail::SystemNode_on_update<T>>();
        registry.on_destroy<T>().template connect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    ecs::SystemNode<T>::~SystemNode()
    {
        auto [lock, registry] = _registry.write();

        registry.on_construct<T>().template disconnect<&detail::SystemNode_on_construct<T>>();
        registry.on_update<T>().template disconnect<&detail::SystemNode_on_update<T>>();
        registry.on_destroy<T>().template disconnect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    inline void ecs::SystemNode<T>::traverse(vsg::Visitor& v)
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        auto [lock, registry] = _registry.read();

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.attach_point);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        super::traverse(v);
    }

    //! Pass-thru for VSG const visitors
    template<class T>
    inline void ecs::SystemNode<T>::traverse(vsg::ConstVisitor& v) const
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        auto [lock, registry] = _registry.read();

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.attach_point);
                if (renderable.node)
                    renderable.node->accept(v);
            });

        super::traverse(v);
    }

    template<class T>
    inline void ecs::SystemNode<T>::compile(vsg::Context& context)
    {
        // Compile the pipelines
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->compile(context);
        }

        // Compile the components
        util::SimpleCompiler vsg_compiler(context);

        auto [lock, registry] = _registry.read();

        registry.view<T>().each([&](auto& c)
            {
                auto& renderable = registry.get<Renderable>(c.attach_point);
                if (renderable.node)
                    renderable.node->accept(vsg_compiler);
            });
    }

    template<class T>
    inline void ecs::SystemNode<T>::traverse(vsg::RecordTraversal& rt) const
    {
        const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);
        auto viewID = rt.getState()->_commandBuffer->viewID;

        // Sort components into render sets by pipeline. If this system doesn't support
        // multiple pipelines, just store them all together in renderSet[0].
        if (pipelineRenderLeaves.empty())
        {
            pipelineRenderLeaves.resize(!pipelines.empty() ? pipelines.size() : 1);
        }

        auto [lock, registry] = _registry.read();

        // Get an optimized view of all this system's components:
        registry.view<T, ActiveState, Visibility>().each([&](const entt::entity entity, auto& component, auto& active, auto& visibility)
            {
                auto& renderable = registry.get<Renderable>(component.attach_point);
                if (renderable.node)
                {
                    auto& leaves = !pipelines.empty() ? pipelineRenderLeaves[featureMask(component)] : pipelineRenderLeaves[0];
                    auto* transform = registry.try_get<Transform>(entity);

                    // if it's visible, queue it up for rendering
                    if (visible(visibility, viewID))
                    {
                        leaves.emplace_back(RenderLeaf{ renderable, transform });
                    }

                    // otherwise if it's invisible but still has a transform, process that transform
                    // so it can calculate its screen-space information (for things like decluttering
                    // and intersection)
                    else if (transform)
                    {
                        transform->push(rt, identity_matrix, false);
                    }
                }

                if (renderable.revision != component.revision)
                {
                    entities_to_update.emplace_back(entity);
                    renderable.revision = component.revision;
                }
            });

        // Time to record all visible components. For each pipeline:
        for (int p = 0; p < pipelineRenderLeaves.size(); ++p)
        {
            if (!pipelineRenderLeaves[p].empty())
            {
                // Bind the Graphics Pipeline for this render set, if there is one:
                if (!pipelines.empty())
                {
                    pipelines[p].commands->accept(rt);
                }

                // Them record each component. If the component has a transform apply it too.
                for (auto& leaf : pipelineRenderLeaves[p])
                {
                    if (leaf.transform)
                    {
                        if (leaf.transform->push(rt, identity_matrix, true))
                        {
                            leaf.renderable.node->accept(rt);
                            leaf.transform->pop(rt);
                        }
                    }
                    else
                    {
                        leaf.renderable.node->accept(rt);
                    }
                }

                // clear out for next time around.
                pipelineRenderLeaves[p].clear();
            }
        }
    }


    template<class T>
    inline void ecs::SystemNode<T>::update(VSGContext& context)
    {
        std::vector<ecs::BuildItem> entities_to_build;

        if (!entities_to_update.empty())
        {
            auto [lock, registry] = _registry.read();

            //TODO: can we just do this during record...?
            for (auto& entity : entities_to_update)
            {
                if (!registry.valid(entity))
                    continue;

                T& component = registry.get<T>(entity);
                Renderable& renderable = registry.get<Renderable>(component.attach_point);

                // either the node doesn't exist yet, or the revision changed.
                // Queue it up for creation.
                ecs::BuildItem item;
                item.entity = entity;
                item.version = registry.current(entity);
                item.component = new T(component);
                item.existing_node = renderable.node;
                item.new_node = {};

                entities_to_build.emplace_back(std::move(item));
            }
        }

        // If we detected any entities that need new nodes, create them now.
        if (!entities_to_build.empty())
        {
            if (SystemNodeBase::factory) // why do I have to qualify this?
            {
                ecs::BuildBatch batch;
                batch.items = std::move(entities_to_build);
                batch.system = this;
                batch.context = &context;

                bool ok = SystemNodeBase::factory->buffers->input.emplace(std::move(batch));

                if (!ok)
                {
                    Log()->warn("Failed to enqueue entity compile job - queue overflow");
                }
            }
        }

        entities_to_update.clear();
    }

    template<typename T>
    void ecs::SystemNode<T>::invokeCreateOrUpdate(BuildItem& item, VSGContext& context) const
    {
        createOrUpdateNode(*static_cast<T*>(item.component), item, context);
    }

    template<typename T>
    void ecs::SystemNode<T>::mergeCreateOrUpdateResults(entt::registry& registry, BuildItem& item, VSGContext& context)
    {
        // if there's a new node AND the entity isn't outdated or deleted,
        // swap it in. This method is called from ECSNode::update().
        if (item.new_node && registry.valid(item.entity) && registry.current(item.entity) == item.version)
        {
            T& component = registry.get<T>(item.entity);
            auto& renderable = registry.get<Renderable>(component.attach_point);

            if (renderable.node != item.new_node)
            {
                // dispose of the old node
                if (renderable.node)
                    context->dispose(renderable.node);

                renderable.node = item.new_node;
            }
        }
    }

    template<class T>
    bool ecs::SystemNode<T>::parseReferencePoint(const GeoPoint& point, SRSOperation& out_xform, vsg::dvec3& out_offset) const
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
    vsg::ref_ptr<vsg::PipelineLayout> ecs::SystemNode<T>::getPipelineLayout(const T& t) const
    {
        return pipelines.empty() ? nullptr : pipelines[featureMask(t)].config->layout;
    }
}
