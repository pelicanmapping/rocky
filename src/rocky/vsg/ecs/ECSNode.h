/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ECS.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/System.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/vsg/ecs/ECSVisitors.h>

namespace ROCKY_NAMESPACE
{
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

        //! Gets a pointer to the first typed system found, or null if not found.
        template<typename T> T* get();

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

    // inline 
    template<typename T> T* ECSNode::get()
    {
        for (auto& system : systems)
        {
            if (auto cast = dynamic_cast<T*>(system))
                return cast;
        }
        return nullptr;
    }





    namespace detail
    {
        class ROCKY_EXPORT SimpleSystemNodeBase :
            public vsg::Inherit<vsg::Compilable, SimpleSystemNodeBase>,
            public System
        {
        protected:
            SimpleSystemNodeBase(Registry& in_registry);

            void update(VSGContext vsgcontext) override;

            inline void requestCompile(vsg::Object* object) const {
                _toCompile->addChild(vsg::ref_ptr<vsg::Object>(object));
            }
            inline void dispose(vsg::Object* object) const {
                _toDispose->addChild(vsg::ref_ptr<vsg::Object>(object));
            }
            inline void requestUpload(vsg::BufferInfo* bi) const {
                _buffersToUpload.emplace_back(bi);
            }
            inline void requestUpload(vsg::BufferInfoList& bil) const {
                _buffersToUpload.insert(_buffersToUpload.end(), bil.begin(), bil.end());
            }
            inline void requestUpload(vsg::ImageInfo* bi) const {
                _imagesToUpload.emplace_back(bi);
            }
            inline void requestUpload(vsg::ImageInfoList& bil) const {
                _imagesToUpload.insert(_imagesToUpload.end(), bil.begin(), bil.end());
            }

        public: // vsg::Compilable

            virtual void compile(vsg::Context& cc) override {
                for (auto& p : _pipelines) {
                    p.commands->compile(cc);
                }
            };

            virtual void traverse(vsg::Visitor&) override;
            virtual void traverse(vsg::ConstVisitor&) const override;

        protected:
            struct Pipeline
            {
                vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> config;
                vsg::ref_ptr<vsg::Commands> commands;
            };
            std::vector<Pipeline> _pipelines;
            bool _pipelinesCompiled = false;
            mutable vsg::ref_ptr<vsg::MatrixTransform> _tempMT;

            // Information specific to one view. Indexed by viewID.
            struct ViewInfo
            {
                // set when the view's SRS changes, which triggers a geometry update for that view.
                bool dirty = false;

                // the last frame that the view was rendered; used to detect a closed view and clean up its resources
                FrameCountType lastFrame = std::numeric_limits<FrameCountType>::max();

                // cached SRS definition for this view.
                // We do not store an actual SRS (SIOF hazard)
                std::string srsDef;
            };
            mutable ViewLocal<ViewInfo> _viewInfo;

            template<class COMPONENT_T, class GEOM_DETAIL_T>
            void handleConstVisitor(vsg::ConstVisitor& visitor) const;

            template<class GEOM_DETAIL_T>
            void handleVisitor(vsg::Visitor& visitor);

        private:
            mutable vsg::ref_ptr<vsg::Objects> _toCompile;
            mutable vsg::ref_ptr<vsg::Objects> _toDispose;
            mutable vsg::BufferInfoList _buffersToUpload;
            mutable vsg::ImageInfoList _imagesToUpload;
        };



        template<class GEOM_DETAIL_T>
        void SimpleSystemNodeBase::handleVisitor(vsg::Visitor& visitor)
        {
            if (status.failed()) return;

            // Supports the CompileTraversal, for one, which needs to compile the node
            // for any new View that appears
            _registry.read([&](entt::registry& reg)
                {
                    reg.view<GEOM_DETAIL_T>().each([&](auto& component)
                        {
                            for (auto& view : component.views)
                            {
                                if (view.root)
                                    view.root->accept(visitor);
                            }
                        });
                });
        }


        template<class COMPONENT_T, class GEOM_DETAIL_T>
        void SimpleSystemNodeBase::handleConstVisitor(vsg::ConstVisitor& visitor) const
        {
            if (status.failed()) return;

            // it might be an ECS visitor, in which case we'll communicate the entity being visited
            auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&visitor);
            std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

            _registry.read([&](entt::registry& reg)
                {
                    reg.view<COMPONENT_T, ActiveState>().each([&](auto entity, auto& comp, auto& active)
                        {
                            auto* geomDetail = reg.try_get<GEOM_DETAIL_T>(comp.geometry);
                            if (geomDetail)
                            {
                                auto& geomView = geomDetail->views[viewID];
                                if (geomView.root)
                                {
                                    if (ecsVisitor)
                                        ecsVisitor->currentEntity = entity;

                                    auto* transformDetail = reg.try_get<TransformDetail>(entity);
                                    if (transformDetail)
                                    {
                                        _tempMT->matrix = transformDetail->views[viewID].model;
                                        _tempMT->children[0] = geomView.root;
                                        _tempMT->accept(visitor);
                                    }
                                    else
                                    {
                                        geomView.root->accept(visitor);
                                    }
                                }
                            }
                        });
                });

            _tempMT->children[0] = nullptr;
        }
    }
}
