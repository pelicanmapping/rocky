/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TransformDetail.h"
#include <rocky/vsg/ShaderDefines.h>
#include <rocky/vsg/VSGUtils.h>
#include <rocky/vsg/ViewDependentState.h>

using namespace ROCKY_NAMESPACE;


namespace
{
    inline vsg::dvec3 xyz(const vsg::dvec4& v)
    {
        return vsg::dvec3(v.x, v.y, v.z);
    }

    inline vsg::dvec3 transform3x3Transpose(const vsg::dmat4& m, const vsg::dvec3& v)
    {
        return vsg::dvec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z);
    }

    [[maybe_unused]] vsg::dvec3 projectVertexToStereographic(
        const vsg::dvec3& position_vs,
        const vsg::dvec2& ellipsoid,
        const vsg::dmat4& viewMatrix)
    {
        vsg::dvec3 sphereCenter_vs = xyz(viewMatrix * vsg::dvec4(0.0, 0.0, 0.0, 1.0));
        vsg::dvec3 look_vs(0.0, 0.0, -1.0);
        vsg::dvec3 planeNormal_vs(0.0, 0.0, 1.0);

        vsg::dvec3 northpoledir_vs = transform3x3Transpose(viewMatrix, vsg::dvec3(0.0, 0.0, 1.0));
        double lat = std::clamp(std::abs(vsg::dot(look_vs, northpoledir_vs)), 0.0, 1.0);
        double R = ellipsoid.x * (1.0 - lat) + ellipsoid.y * lat;
        vsg::dvec3 radius_vs = position_vs - sphereCenter_vs;
        double distance = vsg::length(radius_vs);

        if (distance <= 1e-6)
            return position_vs;

        vsg::dvec3 surface_position_vs = sphereCenter_vs + radius_vs * (R / distance);
        double height = distance - R;

        double b = vsg::dot(sphereCenter_vs, look_vs);
        double discriminant = b * b - (vsg::dot(sphereCenter_vs, sphereCenter_vs) - R * R);

        if (discriminant <= 0.0)
            return surface_position_vs + height * planeNormal_vs;

        double root = std::sqrt(discriminant);
        double t = b - root;
        if (t <= 0.0)
            t = b + root;
        if (t <= 0.0)
            return surface_position_vs + height * planeNormal_vs;

        vsg::dvec3 planeOrigin_vs = look_vs * t;
        vsg::dvec3 antipode_vs = sphereCenter_vs - (planeOrigin_vs - sphereCenter_vs);
        vsg::dvec3 dir_vs = surface_position_vs - antipode_vs;
        double denom = vsg::dot(dir_vs, planeNormal_vs);

        if (denom <= 1e-6)
            return surface_position_vs + height * planeNormal_vs;

        double scale = vsg::dot(planeOrigin_vs - antipode_vs, planeNormal_vs) / denom;
        return antipode_vs + dir_vs * scale + height * planeNormal_vs;
    }

    [[maybe_unused]] vsg::dvec3 projectAnchoredVertexToStereographic(
        const vsg::dvec3& position_vs,
        const vsg::dvec2& ellipsoid,
        const vsg::dmat4& viewMatrix,
        const vsg::dmat4& modelview)
    {
        vsg::dvec3 anchor_vs = xyz(modelview[3]);
        vsg::dvec3 offset_vs = position_vs - anchor_vs;
        vsg::dvec3 projected_anchor_vs = projectVertexToStereographic(anchor_vs, ellipsoid, viewMatrix);
        return projected_anchor_vs + offset_vs;
    }

    inline vsg::dvec4 applyProjection(
        const vsg::dvec4& position_vs,
        const vsg::dvec2& ellipsoid,
        const vsg::dmat4& viewMatrix,
        const vsg::dmat4& projection,
        bool stereographic)
    {
        vsg::dvec4 output = position_vs;
        if (stereographic && projection[3][3] > 0.0)
        {
            auto projected = projectVertexToStereographic(xyz(output), ellipsoid, viewMatrix);
            output.x = projected.x;
            output.y = projected.y;
            output.z = projected.z;
        }
        return output;
    }

    inline vsg::dmat4 to_dmat4(const vsg::mat4& m)
    {
        return vsg::dmat4(
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]);
    }

    inline vsg::dvec2 to_dvec2(const vsg::vec2& v)
    {
        return vsg::dvec2(v.x, v.y);
    }

}



void
TransformDetail::reset(std::uint32_t viewID)
{
    views[viewID].revision = -1;
    views[viewID].cache = {};
}

bool
TransformDetail::traverse(vsg::RecordTraversal& record, const PixelScale* pixelScale)
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
        if (!cache.pos_to_world || sync.position.srs != cache.pos_to_world.from())
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

                bool localMatrixHasScale = false;
                vsg::dvec3 localMatrixScale(1.0, 1.0, 1.0);

                if (ROCKY_MAT4_IS_NOT_IDENTITY(sync.localMatrix))
                {
                    localMatrixScale.x = glm::length(glm::dvec3(sync.localMatrix[0]));
                    localMatrixScale.y = glm::length(glm::dvec3(sync.localMatrix[1]));
                    localMatrixScale.z = glm::length(glm::dvec3(sync.localMatrix[2]));
                    localMatrixHasScale =
                        localMatrixScale.x > 0.0 &&
                        localMatrixScale.y > 0.0 &&
                        localMatrixScale.z > 0.0 &&
                        (localMatrixScale.x != 1.0 || localMatrixScale.y != 1.0 || localMatrixScale.z != 1.0);

                    glm::dmat4 temp;
                    ROCKY_FAST_MAT4_MULT(temp, view.model, sync.localMatrix);
                    view.model = to_vsg(temp);
                }

                view.baseModel = view.model;
                view.baseModelWithoutScale = localMatrixHasScale ?
                    view.baseModel * vsg::scale(1.0 / localMatrixScale.x, 1.0 / localMatrixScale.y, 1.0 / localMatrixScale.z) :
                    view.baseModel;
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
        // Start from the local-scale-neutral base model so we don't compound scale adjustments
        // or apply Transform::localMatrix scale along with PixelScale.
        view.model = view.baseModelWithoutScale;
        model_scale = vsg::length(vsg::dvec3(view.model[0][0], view.model[0][1], view.model[0][2]));
        scaled_radius = sync.radius;
        culling_radius = scaled_radius;

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
    view.position = view.modelview[3];

    // apply the optional screen-space projection:
    auto* vsgView = record.getCommandBuffer()->viewDependentState->view;
    if (!detail::isShadowView(vsgView))
    {
        if (auto vds = viewDependentState(vsgView))
        {
            BufferAccess<RenderParamsGPU> rp(vds->renderParamsBuf);
            if (rp->stereographic)
            {
                auto& view = views[vsgView->viewID];

                view.position = applyProjection(
                    view.position,
                    to_dvec2(to_vsg(rp->ellipsoidAxes)),
                    to_vsg(glm::dmat4(rp->viewMatrix)),
                    view.proj,
                    rp->stereographic != 0);
            }
        }
    }

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
