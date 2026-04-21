/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TransformDetail.h"
#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;

void
TransformDetail::reset(std::uint32_t viewID)
{
    views[viewID].revision = -1;
    views[viewID].cache = {};
}

bool
TransformDetail::update(vsg::RecordTraversal& record, const PixelScale* pixelScale)
{
    if (!sync.position.valid())
        return false;

    auto viewID = record.getCommandBuffer()->viewID;
    auto& view = views[viewID];

    // only if something has changed since last time:
    auto& cache = view.cache;
    auto* state = record.getState();

    bool transform_changed = (view.revision != sync.revision);
    if (transform_changed)
    {
        view.revision = sync.revision;

        // first time through, cache information about the world SRS and ellipsoid for this view.
        if (!cache.pos_to_world)
        {
            if (record.getValue("rocky.worldsrs", cache.world_srs))
            {
                cache.pos_to_world = sync.position.srs.to(cache.world_srs);
                cache.world_ellipsoid = &cache.world_srs.ellipsoid();
            }
        }

        if (cache.pos_to_world)
        {
            glm::dvec3 worldpos;
            if (cache.pos_to_world(sync.position, worldpos))
            {
                if (sync.topocentric && cache.world_srs.isGeocentric())
                {
                    view.model = to_vsg(cache.world_ellipsoid->topocentricToGeocentricMatrix(worldpos));
                }
                else
                {
                    view.model = vsg::translate(worldpos.x, worldpos.y, worldpos.z);
                }

                if (ROCKY_MAT4_IS_NOT_IDENTITY(sync.localMatrix))
                {
                    glm::dmat4 temp;
                    ROCKY_FAST_MAT4_MULT(temp, view.model, sync.localMatrix);
                    view.model = to_vsg(temp);
                }

                view.baseModel = view.model;
            }
        }

        if (!cache.horizon)
        {
            // cache this view's horizon pointer in the local view data
            // so we don't have to look it up every frame
            record.getValue("rocky.horizon", cache.horizon);
        }

        // reset
        view.model = view.baseModel;
    }

    view.proj = state->projectionMatrixStack.top();
    view.viewport = (*state->_commandBuffer->viewDependentState->viewportData)[0];
    auto& mvm = state->modelviewMatrixStack.top();

    // Extract the model matrix's inherent scale (from localMatrix, etc.)
    auto model_scale = vsg::length(vsg::dvec3(view.baseModel[0][0], view.baseModel[0][1], view.baseModel[0][2]));
    auto scaled_radius = model_scale * sync.radius;
    auto culling_radius = scaled_radius;

    if (pixelScale && pixelScale->enabled)
    {
        // Start from the unscaled base model so we don't compound scale adjustments.
        if (!transform_changed) // dont' do it twice
            view.model = view.baseModel;

        double ppu = 0.0; // pixels per world unit at the entity's location

        if (is_perspective_projection_matrix(view.proj))
        {
            // Entity center in view space
            auto eye_pos = mvm * view.model[3];
            auto dist = std::max((-eye_pos.z / eye_pos.w ) - scaled_radius, 1.0);
            if (dist > 0.0)
                ppu = std::abs(view.proj[1][1]) * view.viewport[3] * 0.5 / dist;
        }
        else
        {
            // Ortho: pixels per unit is independent of distance
            ppu = std::abs(view.proj[1][1]) * view.viewport[3] * 0.5;
        }

        // account for system dpr
        ppu *= devicePixelRatio;

        if (ppu > 0.0 && model_scale > 0.0)
        {
            double pixelSize = 2.0 * std::max(scaled_radius, 0.5) * ppu;
            double clamped = std::clamp(pixelSize, (double)pixelScale->minPixels, (double)pixelScale->maxPixels);
            if (clamped != pixelSize)
            {
                auto f = clamped / pixelSize;
                view.model = view.model * vsg::scale(f, f, f);
                culling_radius *= f;
            }
        }
    }
    ROCKY_FAST_MAT4_MULT(view.modelview, mvm, view.model);
    ROCKY_FAST_MAT4_MULT(view.mvp, view.proj, view.modelview);

    view.passingCull = true;

    // Frustum cull (by center point)
    if (sync.frustumCulled)
    {
        auto clip = view.mvp[3] / view.mvp[3][3];

        double tx = 1.0, ty = 1.0, tz = 1.0;
        if (sync.radius > 0.0)
        {
            auto rv = view.modelview[3] + vsg::dvec4(culling_radius, culling_radius, 0, 0);
            auto rc = (view.proj * rv);

            tx += std::abs((rc.x / rc.w) - clip.x);
            ty += std::abs((rc.y / rc.w) - clip.y);
        }

        if (clip.x < -tx || clip.x > tx || clip.y < -ty || clip.y > ty || clip.z < -tz || clip.z > tz)
        {
            view.passingCull = false;
        }
    }

    // horizon cull, if active (geocentric only)
    if (view.passingCull && sync.horizonCulled && cache.horizon && cache.world_srs.isGeocentric())
    {
        auto& horizon = *cache.horizon;
        if (!horizon[viewID].isVisible(view.model[3][0], view.model[3][1], view.model[3][2], culling_radius))
        {
            view.passingCull = false;
        }
    }

    return transform_changed;
}

void
TransformDetail::push(vsg::RecordTraversal& record) const
{
    auto& view = views[record.getCommandBuffer()->viewID];

    auto* state = record.getState();

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(view.modelview);
    state->dirty = true;
    state->pushFrustum();
}

void
TransformDetail::pop(vsg::RecordTraversal& record) const
{
    auto* state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
