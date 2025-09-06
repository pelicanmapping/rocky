/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/ECS.h>
#include <rocky/vsg/ecs/System.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/vsg/VSGUtils.h>
#include <rocky/Utils.h>
#include <thread>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        class SystemNodeBase;

        /**
        * Component that holds a VSG node and its revision (so it can be
        * synchronized with the associated data model). One will typically
        * attach a Renderable to BaseComponent::entity.
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
            std::shared_ptr<BaseComponent> component;
        };

        // Internal structure for a batch of BuildItems associated with a system
        struct BuildBatch
        {
            std::vector<BuildItem> items;
            vsg::ref_ptr<SystemNodeBase> system;
            VSGContext* context = nullptr;
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

            // assert that the component type is move-constructible.
            // This is a C++17 safety mechanism to prevent accidentally disabling move construction if
            // the developer inserts a ~T = default.
            static_assert(std::is_move_constructible_v<T>);

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
            template<typename DVEC3>
            bool parseReferencePoint(const GeoPoint& point, SRSOperation& out_xform, DVEC3& out_offset) const;

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
                Renderable* renderable;
                TransformDetail* transform_detail = nullptr;
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
                //static_assert(std::is_base_of<SystemNodeBase, T>::value, "T must be a subclass of SystemNodeBase");
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
                    system->initialize(runtime);
                }
            }

            //! Update all connected system nodes. This should be invoked once per frame.
            //! @param runtime The runtime object to pass to the systems
            void update(VSGContext& runtime);
        
            std::vector<System*> systems;
            std::vector<std::shared_ptr<System>> non_node_systems;
            Registry registry;
            EntityNodeFactory factory;
        };


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
            r.emplace<detail::Renderable>(new_component.attach_point);

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
                r.emplace<detail::Renderable>(updated_component.attach_point);
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

    template<class T>
    detail::SystemNode<T>::SystemNode(Registry& in_registry) :
        System(in_registry)
    {
        auto [lock, registry] = _registry.write();

        registry.template on_construct<T>().template connect<&detail::SystemNode_on_construct<T>>();
        registry.template on_update<T>().template connect<&detail::SystemNode_on_update<T>>();
        registry.template on_destroy<T>().template connect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    detail::SystemNode<T>::~SystemNode()
    {
        auto [lock, registry] = _registry.write();

        registry.template on_construct<T>().template disconnect<&detail::SystemNode_on_construct<T>>();
        registry.template on_update<T>().template disconnect<&detail::SystemNode_on_update<T>>();
        registry.template on_destroy<T>().template disconnect<&detail::SystemNode_on_destroy<T>>();
    }

    template<class T>
    inline void detail::SystemNode<T>::traverse(vsg::Visitor& v)
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
    inline void detail::SystemNode<T>::traverse(vsg::ConstVisitor& v) const
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
    inline void detail::SystemNode<T>::compile(vsg::Context& context)
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
    inline void detail::SystemNode<T>::traverse(vsg::RecordTraversal& record) const
    {
        const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

        detail::RenderingState rs{
            record.getCommandBuffer()->viewID,
            record.getFrameStamp()->frameCount
        };

        // Sort components into render sets by pipeline. If this system doesn't support
        // multiple pipelines, just store them all together in renderSet[0].
        if (pipelineRenderLeaves.empty())
        {
            pipelineRenderLeaves.resize(!pipelines.empty() ? pipelines.size() : 1);
        }

        auto [lock, registry] = _registry.read();
            

        // Get an optimized view of all this system's components:
        const auto& view = registry.view<T, ActiveState, Visibility>();
        view.each([&](const entt::entity entity, auto& component, auto& active, auto& visibility)
            {
                ROCKY_HARD_ASSERT(component.attach_point != entt::null);
                auto& renderable = registry.get<Renderable>(component.attach_point);
                if (renderable.node)
                {
                    auto& leaves = !pipelines.empty() ? pipelineRenderLeaves[featureMask(component)] : pipelineRenderLeaves[0];
                    auto* transform_detail = registry.try_get<TransformDetail>(entity);

                    // if it's visible, queue it up for rendering
                    if (visible(visibility, rs))
                    {
                        if (transform_detail)
                        {
                            if (transform_detail->visible(rs))
                            {
                                leaves.emplace_back(RenderLeaf{ &renderable, transform_detail });
                            }
                        }
                        else
                        {
                            leaves.emplace_back(RenderLeaf{ &renderable });
                        }
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
                    pipelines[p].commands->accept(record);
                }

                // Them record each component. If the component has a transform apply it too.
                for (auto& leaf : pipelineRenderLeaves[p])
                {
                    if (leaf.transform_detail)
                    {
                        leaf.transform_detail->push(record);
                    }

                    leaf.renderable->node->accept(record);

                    if (leaf.transform_detail)
                    {
                        leaf.transform_detail->pop(record);
                    }
                }

                // clear out for next time around.
                pipelineRenderLeaves[p].clear();
            }
        }
    }


    template<class T>
    inline void detail::SystemNode<T>::update(VSGContext& context)
    {
        std::vector<detail::BuildItem> entities_to_build;

        if (!entities_to_update.empty())
        {
            auto [lock, registry] = _registry.read();

            //TODO: can we just do this during record...?
            for (auto& entity : entities_to_update)
            {
                if (!registry.valid(entity) || !registry.all_of<T>(entity))
                    continue;

                T& component = registry.get<T>(entity);

                auto* renderable = registry.try_get<Renderable>(component.attach_point);
                if (renderable)
                {
                    // either the node doesn't exist yet, or the revision changed.
                    // Queue it up for creation.
                    detail::BuildItem item;
                    item.entity = entity;
                    item.version = registry.current(entity);
                    item.component = std::shared_ptr<BaseComponent>(new T(component));
                    item.existing_node = renderable->node;
                    item.new_node = {};

                    entities_to_build.emplace_back(std::move(item));
                }
                else
                {
                    Log()->warn("Failed to fetch Renderable for component {}",
                        typeid(T).name());
                }
            }
        }

        // If we detected any entities that need new nodes, create them now.
        if (!entities_to_build.empty())
        {
            if (SystemNodeBase::factory) // why do I have to qualify this?
            {
                detail::BuildBatch batch;
                batch.items = std::move(entities_to_build);
                batch.system = this;
                batch.context = &context;

                int count = 0;
                while(!SystemNodeBase::factory->buffers->input.push(batch))
                {
                    //Log()->warn("Failed to enqueue entity compile job - queue overflow - will retry, tries = {}", ++count);
                    std::this_thread::yield();
                }
            }
        }

        entities_to_update.clear();
    }

    template<typename T>
    void detail::SystemNode<T>::invokeCreateOrUpdate(BuildItem& item, VSGContext& context) const
    {
        T component = *static_cast<T*>(item.component.get());
        createOrUpdateNode(component, item, context);
    }

    template<typename T>
    void detail::SystemNode<T>::mergeCreateOrUpdateResults(entt::registry& registry, BuildItem& item, VSGContext& context)
    {
        // if there's a new node AND the entity isn't outdated or deleted,
        // swap it in. This method is called from ECSNode::update().
        if (item.new_node && registry.valid(item.entity) && registry.all_of<T>(item.entity) &&
            registry.current(item.entity) == item.version)
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
    template<typename DVEC3>
    bool detail::SystemNode<T>::parseReferencePoint(const GeoPoint& point, SRSOperation& out_xform, DVEC3& out_offset) const
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
                    out_offset = DVEC3{ world.x, world.y, world.z };
                }
            }
            else
            {
                out_offset = DVEC3{ point.x, point.y, point.z };
            }

            out_xform = SRSOperation(point.srs, worldSRS);
            return true;
        }
        return false;
    }

    template<class T>
    vsg::ref_ptr<vsg::PipelineLayout> detail::SystemNode<T>::getPipelineLayout(const T& t) const
    {
        return pipelines.empty() ? nullptr : pipelines[featureMask(t)].config->layout;
    }
}
