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


    // Render data that can be shared across systems. 
    // VSGContextImpl creates and owns a unique instance of this.
    struct ROCKY_EXPORT SharedRenderData
    {
        SharedRenderData();

        ViewLocal<vsg::ref_ptr<ViewDependentStateEx>> viewDependentState;

        // descriptors shared by all views:
        vsg::ref_ptr<vsg::DescriptorBuffer> mapSettingsBuf;
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

