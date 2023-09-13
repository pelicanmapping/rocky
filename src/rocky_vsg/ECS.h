/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeoTransform.h>
#include <rocky_vsg/engine/Runtime.h>
#include <rocky_vsg/engine/Utils.h>
#include <vsg/vk/Context.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/Node.h>
#include <entt/entt.hpp>
#include <vector>
#include <chrono>

namespace ROCKY_NAMESPACE
{
    //! Entity Component System support
    namespace ECS
    {
        /**
        * Base class for all ECS components.
        */
        class ROCKY_VSG_EXPORT Component
        {
        public:
            //! Component readable name
            std::string name;

            //! Link to another entity in a collection
            entt::entity next_entity = entt::null;

            //! Serialize this component to a JSON string
            virtual JSON to_json() const;
        };

        /**
        * Extends the entt::registry to add some useful functions
        */
        class ROCKY_VSG_EXPORT Registry : public entt::registry
        {
        public:
            //! Provides a simple way to add multiple components of the same type
            //! to an entity, at least from the API's perspective.
            //! T must be an ECS::Component type.
            template<class T>
            inline T& append(entt::entity entity)
            {
                T* comp = try_get<T>(entity);
                if (comp)
                {
                    if (comp->next == entt::null)
                        comp->next = create();
                    return append<T>(comp->next);
                }
                return emplace<T>(entity);
            }
        };

        /**
        * Base class for all ECS systems.
        * A "system" is a module that performs operations on a specific Component type.
        */
        class ROCKY_VSG_EXPORT System
        {
        public:
            //! ECS entity registry
            entt::registry& registry;

            //! Status of the system; check this before using it
            //! to make sure it is properly initialized
            Status status;

        protected:
            System(entt::registry& registry_) :
                registry(registry_) { }
        };
    }

    namespace ECS
    {
        using time_point = std::chrono::steady_clock::time_point;

        /**
        * Node that holds all active ECS Systems as children, so that they
        * can properly respond to VSG visitors.
        */
        class ROCKY_VSG_EXPORT ECSNode : public vsg::Inherit<vsg::Group, ECSNode>
        {
        public:
            ECSNode(entt::registry& registry_) :
                registry(registry_) { }

            //! Entity registry reference
            entt::registry& registry;

            //! Initialize all child systems; call once at startup
            void initialize(Runtime&);
            
            //! Update any child systems that need updating; call once per frame
            void update(Runtime&, time_point);
        };

        /**
        * Base class for a ECS Component that exposes a list of VSG commands.
        */
        class ROCKY_VSG_EXPORT NodeComponent : public Component
        {
        public:
            /**
            * Component initialization parameters for VSG objects
            */
            struct Params
            {
                vsg::ref_ptr<vsg::PipelineLayout> layout;
                vsg::ref_ptr<vsg::Options> readerWriterOptions;
            };

        public:
            //! Subclass implements to create and VSG objects.
            //! Called by the System if the component's node is nullptr.
            virtual void initializeNode(const Params&) { }

            //! Mask of features pertaining to this component instance, if applicable
            virtual int featureMask() const { return 0; }

            //! VSG node that renders this component
            vsg::ref_ptr<vsg::Node> node;

            //! Whether to draw this component
            bool active = true;

            //! Component developers can use this to tie this component's
            //! visiblity to another component. When this is set, "visible"
            //! is ignored.
            bool* active_ptr = &active;
        };

        /**
        * Base class for an ECS System that can live in the scene graph and
        * respond to VSG traversals. It will process all components associated
        * with the system type.
        */
        class ROCKY_VSG_EXPORT SystemNode : 
            public vsg::Inherit<vsg::Compilable, SystemNode>,
            public ECS::System
        {
        public:
            //! Initialize the ECS system (once at startup)
            virtual void initialize(Runtime& runtime) { }

            //! Update the ECS system (once per frame)
            virtual void update(Runtime& runtime, time_point time)
            {
                initializeComponents(runtime);
                tick(runtime, time);
            }

            //! Subclass this to perform per-frame operations on components
            virtual void tick(Runtime& runtime, time_point time) { }

        protected:
            //! Constructor
            SystemNode(entt::registry& registry) :
                ECS::System(registry) { }

            //! Override this to handle any components that need
            //! initial setup
            virtual void initializeComponents(Runtime& rutime) { }
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
        template<class T> // T must inherit from NodeComponent
        class VSG_SystemHelper
        {
        public:
            //! Construct the system helper object.
            VSG_SystemHelper(entt::registry& registry_) :
                registry(registry_) { }

            //! Destruct the helper, expressly destroying any Vulkan objects
            //! that it created immediately
            ~VSG_SystemHelper()
            {
                pipelines.clear();
            }

            // ECS entity registry reference
            entt::registry& registry;

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
            inline void initializeComponents(Runtime&);

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
    struct ROCKY_VSG_EXPORT EntityTransform : public ECS::Component
    {
        EntityTransform() {
            node = GeoTransform::create();
        }

        //! Sets the transform's geoposition
        void setPosition(const GeoPoint& p) {
            node->setPosition(p);
        }

        vsg::ref_ptr<GeoTransform> node;
    };

    /**
    * ECS Component representing a moving entity
    */
    struct EntityMotion : public ECS::Component
    {
        glm::dvec3 velocity;
        glm::dvec3 acceleration;

    private:
        SRSOperation world2pos;
        friend class EntityMotionSystem;
    };

    /**
    * ECS System to process EntityMotion components
    */
    class ROCKY_VSG_EXPORT EntityMotionSystem : public vsg::Inherit<ECS::SystemNode, EntityMotionSystem>
    {
    public:
        //! Constructor
        EntityMotionSystem(entt::registry& r) : 
            vsg::Inherit<ECS::SystemNode, EntityMotionSystem>(r) { }

        //! Called every frame to update transforms
        void tick(Runtime& runtime, ECS::time_point time) override;

    private:
        ECS::time_point last_time = ECS::time_point::min();
    };




    //! Pass-thru for VSG visitors
    template<class T>
    inline void ECS::VSG_SystemHelper<T>::accept(vsg::Visitor& v)
    {
        ROCKY_PROFILE_FUNCTION();

        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](const auto e, auto& component)
            {
                component.node->accept(v);
            });
    }
    
    //! Pass-thru for VSG const visitors
    template<class T>
    inline void ECS::VSG_SystemHelper<T>::accept(vsg::ConstVisitor& v) const
    {
        ROCKY_PROFILE_FUNCTION();

        for (auto& pipeline : pipelines)
        {
            pipeline.commands->accept(v);
        }

        registry.view<T>().each([&](const auto e, auto& component)
            {
                component.node->accept(v);
            });
    }

    template<class T>
    inline void ECS::VSG_SystemHelper<T>::compile(vsg::Context& context)
    {
        ROCKY_PROFILE_FUNCTION();

        // Compile the pipelines
        for (auto& pipeline : pipelines)
        {
            pipeline.commands->compile(context);
        }

        // Compile the components
        util::SimpleCompiler compiler(context);
        auto view = registry.view<T>();
        view.each([&](const auto entity, auto& component)
            {
                component.node->accept(compiler);
            });
    }

    template<class T>
    inline void ECS::VSG_SystemHelper<T>::record(vsg::RecordTraversal& rt) const
    {
        ROCKY_PROFILE_FUNCTION();

        // Get an optimized view of all this system's components:
        auto view = registry.view<T>();

        using Entry = struct {
            const T& component;
            entt::entity entity;
        };

        // Sort components into render sets by pipeline.
        // If this system doesn't support multiple pipelines, just 
        // store them all together
        std::vector<std::vector<Entry>> render_set(!pipelines.empty() ? pipelines.size() : 1);

        view.each([&](const entt::entity entity, const T& component)
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

                    // Otherwise, it's new and needs intialization, so queue it up for that.
                    else
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
                    auto* xform = registry.try_get<EntityTransform>(e.entity);
                    if (xform)
                    {
                        if (xform->node->push(rt))
                        {
                            e.component.node->accept(rt);
                            xform->node->pop(rt);
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

    template<class T>
    inline void ECS::VSG_SystemHelper<T>::initializeComponents(Runtime& runtime)
    {
        ROCKY_PROFILE_FUNCTION();

        // Components with VSG elements need to create and compile those
        // elements before we can render them. These get put on the 
        // initialization list by the record traversal.
        if (!entities_to_initialize.empty())
        {
            NodeComponent::Params params;
            params.readerWriterOptions = runtime.readerWriterOptions;

            for (auto& entity : entities_to_initialize)
            {
                auto& component = registry.get<T>(entity);
                if (!component.node)
                {
                    // if we're using pipelines, find the one matching this
                    // component's feature set:
                    if (pipelines.empty())
                        params.layout = { };
                    else
                        params.layout = pipelines[component.featureMask()].config->layout;

                    // Tell the component to create its VSG node(s)
                    component.initializeNode(params);

                    // TODO: Replace this will some error checking
                    ROCKY_SOFT_ASSERT(component.node);
                }

                // compile the vulkan objects
                if (component.node)
                {
                    runtime.compile(component.node);
                }
            }

            // reset the list for the next frame
            entities_to_initialize.clear();
        }
    }
}

#define ROCKY_VSG_SYSTEM_HELPER(TYPE, MEMBER) \
    ROCKY_NAMESPACE::ECS::VSG_SystemHelper<TYPE> MEMBER; \
    void accept(vsg::Visitor& v) override { MEMBER.accept(v); } \
    void accept(vsg::ConstVisitor& v) const override { MEMBER.accept(v); } \
    void compile(vsg::Context& context) override { MEMBER.compile(context); } \
    void traverse(vsg::RecordTraversal& rt) const override { MEMBER.record(rt); } \
    void initializeComponents(Runtime& runtime) override { MEMBER.initializeComponents(runtime); }
