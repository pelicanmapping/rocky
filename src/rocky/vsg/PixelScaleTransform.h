/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Transform that lets you set an object's size in pixels rather than
    * scene units (e.g. meters). Good for text or other billboarded
    * screen-space geometry.
    *
    * This won't work great for multi-threaded record traversals..
    */
    class /*ROCKY_EXPORT*/ PixelScaleTransform : public vsg::Inherit<vsg::Transform, PixelScaleTransform>
    {
    public:
        //! Whether to undo any rotation found in the origial model view matrix;
        //! This will effectively billboard the geometry.
        bool enabled = true;
        bool unrotate = false;
        bool snap = false;
        float unitSize = 1.0f;
        float renderSize = 1.0f;

        void accept(vsg::RecordTraversal& rt) const override
        {
            if (enabled)
            {
                // Calculate the scale factor that will scale geometry from pixel space to model space
                auto& state = *rt.getState();
                auto& viewport = state._commandBuffer->viewDependentState->viewportData->at(0);

                auto& mvm = state.modelviewMatrixStack.top();
                auto& proj = state.projectionMatrixStack.top();

                if (is_perspective_projection_matrix(proj))
                {
                    // lodDistance factors in the MVM scale via rot_scale; multiplying back
                    // by the extracted scale cancels it, leaving a depth-proportional pixel scale.
                    auto scale = vsg::length(vsg::dvec3(mvm[0][0], mvm[0][1], mvm[0][2]));
                    auto d = state.lodDistance(vsg::dsphere(0.0, 0.0, 0.0, 0.5)) / viewport[3];
                    _matrix = vsg::scale(scale * d);
                }

                else
                {
                    // Ortho: world units per pixel = (top-bottom)/viewport_height.
                    // mv * scale(d) naturally incorporates the MVM scale, so no pre-multiplication needed.
                    auto d = 2.0 / (-proj[1][1] * viewport[3]);
                    _matrix = vsg::scale(d);
                }

                if (unrotate)
                {
                    auto rotation = quaternion_from_unscaled_matrix<vsg::dquat>(mvm);
                    _matrix = _matrix * vsg::rotate(vsg::inverse(rotation));
                }

                if (snap)
                {
                    auto mvp = state.projectionMatrixStack.top() * state.modelviewMatrixStack.top();
                    auto clip = mvp * _matrix;

                    clip[3][0] = 0.5 * (clip[3][0] / clip[3][3]) * viewport[2];
                    clip[3][1] = 0.5 * (clip[3][1] / clip[3][3]) * viewport[3];
                    clip[3][0] = 2.0 * (floor(clip[3][0]) / viewport[2]) * clip[3][3];
                    clip[3][1] = 2.0 * (floor(clip[3][1]) / viewport[3]) * clip[3][3];

                    _matrix = vsg::inverse(mvp) * clip;
                }
            }

            rt.apply(*this);
        };

        vsg::dmat4 transform(const vsg::dmat4& mv) const override
        {
            return enabled ? mv * _matrix : mv;
        }

    private:
        mutable vsg::dmat4 _matrix;
    };

    /**
    * Group that applies a viewport-space transform to its children.
    * (Children's vertices are treated as in viewport pixel space.)
    */
    class ROCKY_EXPORT ScreenSpaceGroup : public vsg::Inherit<vsg::Group, ScreenSpaceGroup>
    {
    public:
        //! Snap to nearest pixel
        bool snap = true;

        //! Overall scale factor
        double scale = 1.0;

        void traverse(vsg::RecordTraversal& rt) const override
        {
            auto& state = *rt.getState();
            auto& viewport = state._commandBuffer->viewDependentState->viewportData->at(0);

            auto ortho = vsg::orthographic(0.0, (double)viewport[2]-1.0, 0.0, (double)viewport[3]-1.0, -1.0, 1.0);

            auto mvp = state.projectionMatrixStack.top() * state.modelviewMatrixStack.top();
            auto clip = mvp * vsg::dvec4(0,0,0,1);
            auto x = ((clip.x / clip.w) * 0.5 + 0.5) * viewport[2] + viewport[0];
            auto y = ((clip.y / clip.w) * 0.5 + 0.5) * viewport[3] + viewport[1];
            
            // snap position to the nearest pixel to prevent "swimming"
            vsg::dmat4 modelview = vsg::dmat4(1);
            modelview[3][0] = snap ? std::floor(x) : x;
            modelview[3][1] = snap ? std::floor(viewport[3] - y) : viewport[3] - y;

            double dpr = 1.0;
            rt.getValue("rocky.dpr", dpr);
            modelview[0][0] = modelview[1][1] = scale * dpr;

            state.projectionMatrixStack.push(ortho);
            state.modelviewMatrixStack.push(modelview);
            state.dirty = true;
            state.pushFrustum();

            Inherit::traverse(rt);

            state.popFrustum();
            state.modelviewMatrixStack.pop();
            state.projectionMatrixStack.pop();
            state.dirty = true;
        }
    };
}
