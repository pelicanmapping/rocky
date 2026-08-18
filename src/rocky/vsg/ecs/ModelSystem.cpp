/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "ModelSystem.h"
#include "OverlayRenderContext.h"
#include "ECSVisitors.h"
#include "TransformDetail.h"
#include <rocky/ecs/Model.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/vsg/FutureNode.h>
#include <filesystem>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

RenderTextureSourceStatus ModelSystemNode::renderTextureSourceStatus(entt::registry& reg, entt::entity entity) const
{
    if (!reg.any_of<Model>(entity))
        return {};
    const auto& model = reg.get<Model>(entity);
    if (model.error)
        return { RenderTextureSourceStatus::State::Failed, model.error->message };
    auto* detail = reg.try_get<detail::ModelDetail>(entity);
    if (!detail || !detail->node)
        return { RenderTextureSourceStatus::State::Waiting, "Waiting for model data", true };
    if (auto* future = dynamic_cast<FutureNode*>(detail->node.get()))
        return future->resolve() ? RenderTextureSourceStatus{} :
            RenderTextureSourceStatus{ RenderTextureSourceStatus::State::Waiting, "Waiting for model load", true };
    return {};
}

void ModelSystemNode::expandRenderTextureBounds(
    entt::registry& reg, entt::entity entity, RenderTextureBounds& bounds, const SRS& worldSRS, bool applySourceTransform)
{
    auto* modelDetail = reg.try_get<detail::ModelDetail>(entity);
    if (!modelDetail || !modelDetail->node || !worldSRS.valid() ||
        renderTextureSourceStatus(reg, entity).state != RenderTextureSourceStatus::State::Ready)
        return;

    vsg::ComputeBounds cb;
    modelDetail->node->accept(cb);
    if (!cb.bounds) return;
    const auto& b = cb.bounds;
    for (int ix = 0; ix < 2; ++ix)
        for (int iy = 0; iy < 2; ++iy)
            for (int iz = 0; iz < 2; ++iz)
                expandRenderTextureSourcePoint(reg, entity, bounds, worldSRS, glm::dvec3(
                    ix ? b.max.x : b.min.x,
                    iy ? b.max.y : b.min.y,
                    iz ? b.max.z : b.min.z), worldSRS, applySourceTransform);
}

void ModelSystemNode::contributeRenderTextureRevision(
    entt::registry& reg, entt::entity entity, RenderTextureRevision& revision)
{
    // URI and local-matrix changes both affect the model's pixels and bounds.
    detail::combineRenderTextureComponentBoth(
        revision, reg.try_get<Model>(entity));
}


ModelSystemNode::ModelSystemNode(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<Model>().connect<&ModelSystemNode::on_construct_Model>(*this);
            r.on_destroy<Model>().connect<&ModelSystemNode::on_destroy_Model>(*this);
            r.on_update<Model>().connect<&ModelSystemNode::on_update_Model>(*this);

            auto e = r.create();
            r.emplace<Model::Dirty>(e);

            std::vector<entt::entity> existing;
            r.view<Model>().each([&](auto entity, auto&) { existing.push_back(entity); });
            for (auto entity : existing)
                on_construct_Model(r, entity);
        });
}

ModelSystemNode::~ModelSystemNode()
{
    _registry.write([&](entt::registry& r)
        {
            r.on_construct<Model>().disconnect<&ModelSystemNode::on_construct_Model>(*this);
            r.on_destroy<Model>().disconnect<&ModelSystemNode::on_destroy_Model>(*this);
            r.on_update<Model>().disconnect<&ModelSystemNode::on_update_Model>(*this);
        });
}

void
ModelSystemNode::on_construct_Model(entt::registry& r, entt::entity e)
{
    // TODO: put this in a utility function somewhere
    // common components that may already exist on this entity:
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    (void)r.get_or_emplace<ModelDetail>(e);
    Model::dirty(r, e);
}

void
ModelSystemNode::on_destroy_Model(entt::registry& r, entt::entity e)
{
    if (auto* detail = r.try_get<ModelDetail>(e))
        dispose(detail->node);
    r.remove<ModelDetail>(e);
}

void
ModelSystemNode::on_update_Model(entt::registry& r, entt::entity e)
{
    Model::dirty(r, e);
}

void
ModelSystemNode::initialize(VSGContext vsgcontext)
{
    // nop
}

void
ModelSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    auto vp = record.getCommandBuffer()->viewDependentState->view->camera->getViewport();
    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount,
        { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
    };

    auto renderRequest = getRenderRequest(record);

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            auto renderEntity = [&](auto entity, auto& model, auto& det, auto& active, auto& visibility)
                {
                    if (!renderRequest.contains(entity))
                        return;

                    if (auto* participation = reg.try_get<RenderParticipation>(entity))
                    {
                        if ((renderRequest.purpose == RenderPurpose::Main && !participation->mainView) ||
                            (renderRequest.purpose == RenderPurpose::RenderTexture && !participation->renderTexture))
                            return;
                    }

                    if (det.node)
                    {
                        if (model.radius <= 0.0)
                        {
                            // if neccessary, compute the bounding radius and store it in the component.
                            vsg::ComputeBounds cb;
                            det.node->accept(cb);
                            if (cb.bounds)
                            {
                                model.radius = vsg::length(cb.bounds.max - cb.bounds.min) * 0.5;
                                // ...and in the transform if there is one.
                                if (auto* xform = reg.try_get<Transform>(entity))
                                {
                                    xform->radius = model.radius;
                                }
                            }
                        }

                        if (visible(visibility, rs))
                        {
                            auto* xformDetail = reg.try_get<TransformDetail>(entity);
                            bool useTransform = !(renderRequest.purpose == RenderPurpose::RenderTexture && renderRequest.ignoreSourceTransforms);
                            if (xformDetail)
                            {
                                bool passes = (renderRequest.purpose == RenderPurpose::RenderTexture) || xformDetail->views[rs.viewID].passingCull;
                                if (useTransform && passes)
                                {
                                    _drawList.emplace_back(det.node, xformDetail);
                                }
                                else if (!useTransform)
                                {
                                    _drawList.emplace_back(det.node, nullptr);
                                }
                            }
                            else
                            {
                                _drawList.emplace_back(det.node, nullptr);
                            }
                        }
                    }
                };

            if (renderRequest.purpose == RenderPurpose::RenderTexture && !renderRequest.sources.empty())
            {
                for (auto source : renderRequest.sources)
                {
                    if (reg.all_of<Model, ModelDetail, ActiveState, Visibility>(source))
                    {
                        auto&& [model, detail, active, visibility] =
                            reg.get<Model, ModelDetail, ActiveState, Visibility>(source);
                        renderEntity(source, model, detail, active, visibility);
                    }
                }
            }
            else
            {
                auto iter = reg.view<Model, ModelDetail, ActiveState, Visibility>();
                iter.each(renderEntity);
            }

            // Render collected data.
            for (auto& drawable : _drawList)
            {
                if (drawable.xformDetail)
                {
                    drawable.xformDetail->push(record);
                }

                drawable.node->accept(record);

                if (drawable.xformDetail)
                {
                    drawable.xformDetail->pop(record);
                }
            }

            _drawList.clear();
        });
}

void
ModelSystemNode::traverse(vsg::Visitor& visitor)
{
    // Supports the CompileTraversal, for one, which needs to compile the node
    // for any new View that appears
    _registry.read([&](entt::registry& reg)
        {
            reg.view<ModelDetail>().each([&](auto& det)
                {
                    if (det.node)
                    {
                        det.node->accept(visitor);
                    }
                });
        });

    Inherit::traverse(visitor);
}

void
ModelSystemNode::traverse(vsg::ConstVisitor& visitor) const
{
    if (status.failed()) return;

    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&visitor);
    std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

    _registry.read([&](entt::registry& reg)
        {
            auto iter = reg.view<ModelDetail, ActiveState>();

            iter.each([&](auto entity, auto& modelDetail, auto& active)
                {
                    if (modelDetail.node)
                    {
                        if (ecsVisitor)
                            ecsVisitor->currentEntity = entity;

                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {                            
                            _tempMT->matrix = transformDetail->views[viewID].model;
                            _tempMT->children[0] = modelDetail.node;
                            _tempMT->accept(visitor);
                        }
                        else
                        {
                            modelDetail.node->accept(visitor);
                        }
                    }
                });
        });

    Inherit::traverse(visitor);
}

void
ModelSystemNode::compile(vsg::Context& cc)
{
    if (firstCompileForView(cc))
    {
        _registry.read([&](entt::registry& reg)
            {
                reg.view<ModelDetail>().each([&](auto& m)
                {
                    if (m.node)
                        requestCompile(m.node);
                });
            });
    }
    Inherit::compile(cc);
}

void
ModelSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

    // process any objects marked dirty
    _registry.read([&](entt::registry& reg)
        {
            Model::eachDirty(reg, [&](entt::entity entity)
                {
                    auto&& [model, det] = reg.get<Model, ModelDetail>(entity);
                    model.error.reset();

                    if (det.node)
                        dispose(det.node);
                    det.node = {};

                    auto loadModel = [model(model), io(vsgcontext->io), options(vsgcontext->readerWriterOptions)](Cancelable& c)
                        -> Result<vsg::ref_ptr<vsg::Node>>
                        {
                            if (c.canceled())
                                return Failure_OperationCanceled;

                            auto rr = model.uri.read(io);
                            if (!rr)
                                return rr.error();

                            std::filesystem::path path(model.uri.full());
                            auto opts = vsg::clone(options);
                            opts->extensionHint = path.extension();
                            if (opts->extensionHint.empty())
                                opts->extensionHint = rr.value().content.type;
                            std::istringstream buf(rr.value().content.data);

                            auto node = vsg::read_cast<vsg::Node>(buf, opts);
                            if (!node)
                                return Failure("vsg::read_cast failed to parse data (type not supported?)");

                            if (model.localMatrix.has_value())
                            {
                                auto mt = vsg::MatrixTransform::create();
                                mt->matrix = to_vsg(model.localMatrix.value());
                                mt->addChild(node);
                                node = mt;
                            }

                            return node;
                        };

                    // start loading in the background:
                    auto& j = vsgcontext->io.services().jobs;
                    jobs::context context{ model.uri.full(), j.get_pool("rocky::ModelSystem", 2) };

                    LoadRecord record;
                    record.promise = j.dispatch(loadModel, context);
                    record.entity = entity;
                    record.revision = model.componentRevision();

                    _loaders.emplace(std::move(record));

                    // and return a "future node" to hold the result.
                    det.node = FutureNode::create(record.promise, vsgcontext);
                });
        });

    // clean out any finished loaders and propagate any errors.
    while (!_loaders.empty())
    {
        auto& entry = _loaders.front();
        if (entry.promise.available())
        {
            if (entry.promise->failed())
            {
                auto&& reader = _registry.read();
                if (auto* model = reader->try_get<Model>(entry.entity);
                    model && model->componentRevision() == entry.revision)
                    model->error = entry.promise->error();
            }
            _loaders.pop();
        }
        else if (entry.promise.empty())
        {
            _loaders.pop();
        }
        else
        {
            break;
        }
    }

    Inherit::update(vsgcontext);
}
