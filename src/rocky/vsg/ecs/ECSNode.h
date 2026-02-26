/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ECS.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/System.h>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        class ROCKY_EXPORT SimpleSystemNodeBase : 
            public vsg::Inherit<vsg::Compilable, SimpleSystemNodeBase>,
            public System
        {
        protected:
            SimpleSystemNodeBase(Registry& in_registry);

            void update(VSGContext vsgcontext) override;

            inline void requestCompile(vsg::Object* object) {
                _toCompile->addChild(vsg::ref_ptr<vsg::Object>(object));
            }
            inline void dispose(vsg::Object* object) {
                _toDispose->addChild(vsg::ref_ptr<vsg::Object>(object));
            }
            inline void requestUpload(vsg::BufferInfo* bi) {
                _buffersToUpload.emplace_back(bi);
            }
            inline void requestUpload(vsg::BufferInfoList& bil) {
                _buffersToUpload.insert(_buffersToUpload.end(), bil.begin(), bil.end());
            }
            inline void requestUpload(vsg::ImageInfo* bi) {
                _imagesToUpload.emplace_back(bi);
            }
            inline void requestUpload(vsg::ImageInfoList& bil) {
                _imagesToUpload.insert(_imagesToUpload.end(), bil.begin(), bil.end());
            }

        public: // vsg::Compilable

            virtual void compile(vsg::Context& cc) override {
                for (auto& p : _pipelines) {
                    p.commands->compile(cc);
                }
            };

        protected:
            struct Pipeline
            {
                vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> config;
                vsg::ref_ptr<vsg::Commands> commands;
            };
            std::vector<Pipeline> _pipelines;
            bool _pipelinesCompiled = false;


        private:
            vsg::ref_ptr<vsg::Objects> _toCompile;
            vsg::ref_ptr<vsg::Objects> _toDispose;
            vsg::BufferInfoList _buffersToUpload;
            vsg::ImageInfoList _imagesToUpload;
        };
    }

    /**
    * VSG Group node that contains systems.
    */
    class ROCKY_EXPORT ECSNode : public vsg::Inherit<vsg::Group, ECSNode>
    {
    public:
        //! Construct
        //! @param reg The entity registry
        ECSNode(Registry& reg);

        //! Construct, optionally adding default systems
        ECSNode(Registry& reg, bool addDefaultSystems);

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
        void initialize(VSGContext vsgcontext);

        //! Update all connected system nodes. This should be invoked once per frame.
        //! @param runtime The runtime object to pass to the systems
        void update(VSGContext vsgcontext);

        std::vector<System*> systems;
        std::vector<std::shared_ptr<System>> non_node_systems;
        Registry registry;
    };

}
