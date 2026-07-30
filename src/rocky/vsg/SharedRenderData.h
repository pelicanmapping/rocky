/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ViewDependentState.h>
#include <rocky/Rendering.h>

namespace ROCKY_NAMESPACE
{
    //! Holds any terrain-wide textures and uniforms.
    struct MapSettingsGPU
    {
        vsg::vec2 ellipsoidAxes{ 1.0, 1.0 };
        float _padding[2];
    };
    static_assert(sizeof(MapSettingsGPU) % 16 == 0, "MapDescriptors::Uniforms must be a multiple of 16 bytes in size");

    struct DecalGPU
    {
        glm::fmat4 mvm;
        glm::fmat4 mvmInverse; // GPU only
        union {
            glm::float32 halfX;
            glm::float32 zMin;
        };
        union {
            glm::float32 halfY;
            glm::float32 zMax;
        };
        union {
            glm::float32 halfZ;
            glm::float32 cullingRadius;
        };
        union {
            std::int32_t textureIndex = -1; // negative means no texture
            std::int32_t count; // used by entry 0 as total decal count
        };
        float distance = 0.0f; // > 0 = persp
        float tanHalfFovY = 0.0f;
        float aspect = 1.0f;
        float opacity = 1.0f;
    };
    static_assert(sizeof(DecalGPU) % 16 == 0, "DecalGPU must be 16-byte aligned");

    // Render data that can be shared across systems. 
    // VSGContextImpl creates and owns a unique instance of this.
    struct ROCKY_EXPORT SharedRenderData
    {
        SharedRenderData();

        ViewLocal<vsg::ref_ptr<ViewDependentStateEx>> viewDependentState;

        // descriptors shared by all views:
        vsg::ref_ptr<vsg::DescriptorBuffer> mapSettingsBuf;
        vsg::ref_ptr<vsg::DescriptorBuffer> decalsBuf;
        vsg::ref_ptr<vsg::DescriptorImage> decalTextures;

        //! whether any of the shared descriptors (above) has changed
        //! since the last check:
        inline bool sharedDescriptorsChanged(Revision& mine) const {
            bool changed = mine != revision;
            mine = revision;
            return changed;
        }

        void dirtySharedDescriptors();

        void rebuildVdsDescriptorSet(ViewIDType viewID, ObjectLifecycle*);

        Revision revision = 0;
    };
}

