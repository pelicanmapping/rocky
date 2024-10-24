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

            //! Link to another entity in a collection
            entt::entity next_entity = entt::null;

            //! Serialize this component to a JSON string
            virtual std::string to_json() const;
        };

        /**
        * Base class for a ECS Component that manifests itself as an VSG node.
        */
        class ROCKY_EXPORT NodeComponent : public Component
        {
        public:
            //! Set a reference point for geometry. For preventing precision jitter.
            //! @param p The reference point
            //! @return The operation that can transform data from the reference point's
            //!   SRS into mesh vertices you can add to a geometry attached to this component.
            SRSOperation setReferencePoint(const GeoPoint& p);

        public:
            //! Subclass implements to create and VSG objects.
            //! Called by the System if the component's node is nullptr.
            //void initializeNode(const Params& params, Args&& ...args) { }

            //! Mask of features pertaining to this component instance, if applicable
            virtual int featureMask() const { return 0; }

            //! VSG node that renders this component
            vsg::ref_ptr<vsg::Node> node;

            //! Whether to draw this component
            bool active = true;

            //! Whether to reinitialize this component's node
            bool nodeDirty = false;

            //! Component developers can use this to tie this component's
            //! visiblity to another component. When this is set, "visible"
            //! is ignored.
            bool* active_ptr = &active;

            //! Developer may override this to customize refresh behavior
            virtual void dirty()
            {
                nodeDirty = true;
            }

        protected:
            vsg::dvec3 refPoint;
        };

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
                initializeNewComponents(runtime);
            }

        protected:
            System(entt::registry& in_registry) :
                registry(in_registry) { }

            //! Override this to handle any components that need initial setup
            virtual void initializeNewComponents(Runtime& rutime) { }
        };

        /**
        * Base class for VSG node that mirrors an ECS System.
        * This node will live in the VSG scene graph and will be able to receive and
        * respond to VSG traversals, and be responsible for rendering graphics
        * assoicated with the system's components.
        */
        class ROCKY_EXPORT SystemNode :
            public vsg::Inherit<vsg::Compilable, SystemNode>,
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
            SystemNode(entt::registry& registry_) :
                System(registry_) { }
        };

        /**
        * VSG Group node whose children are VSG_System objects. It can also hold/manager
        * non-node systems.
        */
        class SystemsGroup : public vsg::Inherit<vsg::Group, SystemsGroup>
        {
        public:
            void add(vsg::ref_ptr<SystemNode> system)
            {
                addChild(system);
                systems.emplace_back(system.get());
            }

            void add(std::shared_ptr<System> system)
            {
                non_node_systems.emplace_back(system);
                systems.emplace_back(system.get());
            }

            //! Initialize of all connected system nodes. This should be invoked
            //! any time a new viewer is created.
            void initialize(Runtime& runtime)
            {
                for (auto& system : systems)
                {
                    system->initializeSystem(runtime);
                }
            }

            //! Update all connected system nodes. This should be invoked once per frame.
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


        /**
        * Helper class for making systems that operate on Component types
        * with VSG nodes in them. VSG nodes respond to many types of traversals
        * like record, compute bounds, intersect... and this system-helper
        * object will allow these traversals to hit those components even 
        * though they aren't in the scene graph. It will also facilitate grouping
        * each component under the Graphics Pipeline appropiate for its
        * rendering properties.
        */
        template<class COMPONENT>
        class SystemNodeHelper
        {
            static_assert(std::is_base_of<NodeComponent, COMPONENT>::value, "COMPONENT must be derived from NodeComponent");

        public:
            //! Construct the system helper object.
            SystemNodeHelper(entt::registry& registry_) :
                registry(registry_) { }

            //! Destruct the helper, expressly destroying any Vulkan objects
            //! that it created immediately
            ~SystemNodeHelper()
            {
                pipelines.clear();
            }

            // ECS entity registry reference
            entt::registry& registry;

            // component initialization function to be installed by the system
            std::function<void(COMPONENT&, SystemNode::InitContext&)> initializeComponent;

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

            // list of entities whose components require some kind of VSG initialization
            mutable std::vector<entt::entity> entities_to_initialize;

            // looks for any new components that need VSG initialization
            inline void initializeNewComponents(Runtime&);

            // Hooks to expose systems and components to VSG visitors.
            inline void accept(vsg::Visitor& v);
            inline void accept(vsg::ConstVisitor& v) const;
            inline void compile(vsg::Context&);
            inline void record(vsg::RecordTraversal&) const;
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

    private:
        SRSOperation world2pos;
        SRSOperation pos2world;
        friend class EntityMotionSystem;
    };


    /**
    * ECS System to process Motion components
    */
    class ROCKY_EXPORT EntityMotionSystem : public ECS::System
    {
    public:
        //! Constructor
        static std::shared_ptr<EntityMotionSystem> create(entt::registry& r) {
            return std::shared_ptr<EntityMotionSystem>(new EntityMotionSystem(r));
        }

        //! Called to update the transforms
        void update(Runtime& runtime) override;

    private:
        //! Constructor
        EntityMotionSystem(entt::registry& r) :
            ECS::System(r) { }

        ECS::time_point last_time = ECS::time_point::min();
    };



    //! Pass-thru for VSG visitors
    template<class COMPONENT>
    inline void ECS::SystemNodeHelper<COMPONENT>::accept(vsg::Visitor& v)
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<COMPONENT>().each([&](const auto e, auto& component)
            {
                if (component.node)
                    component.node->accept(v);
            });
    }
    
    //! Pass-thru for VSG const visitors
    template<class COMPONENT>
    inline void ECS::SystemNodeHelper<COMPONENT>::accept(vsg::ConstVisitor& v) const
    {
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<COMPONENT>().each([&](const auto e, auto& component)
            {
                if (component.node)
                    component.node->accept(v);
            });
    }

    template<class COMPONENT>
    inline void ECS::SystemNodeHelper<COMPONENT>::compile(vsg::Context& context)
    {
        // Compile the pipelines
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->compile(context);
        }

        // Compile the components
        util::SimpleCompiler compiler(context);
        auto view = registry.view<COMPONENT>();
        view.each([&](const auto entity, auto& component)
            {
                component.node->accept(compiler);
            });
    }

    template<class COMPONENT>
    inline void ECS::SystemNodeHelper<COMPONENT>::record(vsg::RecordTraversal& rt) const
    {
        const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

        // Get an optimized view of all this system's components:
        auto view = registry.view<COMPONENT>();

        using Entry = struct {
            const COMPONENT& component;
            entt::entity entity;
        };

        // Sort components into render sets by pipeline.
        // If this system doesn't support multiple pipelines, just 
        // store them all together
        std::vector<std::vector<Entry>> render_set(!pipelines.empty() ? pipelines.size() : 1);

        view.each([&](const entt::entity entity, const COMPONENT& component)
            {
                // Is the component visible?
                if (*component.active_ptr)
                {
                    // Does it have a VSG node? If so, queue it up under the
                    // appropriate pipeline.
                    if (component.node)
                    {
                        auto& rs = !pipelines.empty() ? render_set[component.featureMask()] : render_set[0];
                        rs.emplace_back(Entry{ component, entity });
                    }

                    if (!component.node || component.nodeDirty)
                    {
                        entities_to_initialize.push_back(entity);
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
                            e.component.node->accept(rt);
                            xform->pop(rt);
                        }
                    }
                    else
                    {
                        e.component.node->accept(rt);
                    }
                }
            }
        }
    }

    template<class COMPONENT>
    inline void ECS::SystemNodeHelper<COMPONENT>::initializeNewComponents(Runtime& runtime)
    {
        // Components with VSG elements need to create and compile those
        // elements before we can render them. These get put on the 
        // initialization list by the record traversal.
        if (!entities_to_initialize.empty())
        {
            SystemNode::InitContext context
            {
                {}, // will fill this in below
                runtime.readerWriterOptions,
                runtime.sharedObjects
            };

            for (auto& entity : entities_to_initialize)
            {
                auto& component = registry.get<COMPONENT>(entity);

                // If it's marked dirty, dispose of it properly
                if (component.node && component.nodeDirty)
                {
                    runtime.dispose(component.node);
                    component.node = nullptr;
                }

                if (!component.node)
                {
                    // if we're using pipelines, find the one matching this
                    // component's feature set:
                    if (pipelines.empty())
                        context.layout = { };
                    else
                        context.layout = pipelines[component.featureMask()].config->layout;

                    // Tell the component to create its VSG node(s)
                    if (initializeComponent)
                    {
                        initializeComponent(component, context);
                    }

                    // TODO: Replace this will some error checking
                    ROCKY_SOFT_ASSERT(component.node);
                }

                // compile the vulkan objects
                if (component.node)
                {
                    runtime.compile(component.node);
                }

                component.nodeDirty = false;
            }

            // reset the list for the next frame
            entities_to_initialize.clear();
        }
    }
}

#define ROCKY_SYSTEMNODE_HELPER(COMPONENT, MEMBER) \
    ROCKY_NAMESPACE::ECS::SystemNodeHelper<COMPONENT> MEMBER; \
    void accept(vsg::Visitor& v) override { MEMBER.accept(v); } \
    void accept(vsg::ConstVisitor& v) const override { MEMBER.accept(v); } \
    void compile(vsg::Context& context) override { MEMBER.compile(context); } \
    void traverse(vsg::RecordTraversal& rt) const override { MEMBER.record(rt); } \
    void initializeNewComponents(Runtime& runtime) override { MEMBER.initializeNewComponents(runtime); }
