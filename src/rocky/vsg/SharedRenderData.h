/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ShaderDefines.h>
#include <rocky/vsg/ViewDependentState.h>
#include <rocky/Rendering.h>

namespace ROCKY_NAMESPACE
{
    // Render data that can be shared across systems. 
    // VSGContextImpl creates and owns a unique instance of this.
    struct ROCKY_EXPORT SharedRenderData
    {
        static constexpr std::uint32_t DEFAULT_PROJECTED_TEXTURE_CAPACITY = MAX_NUM_DECAL_TEXTURES;

        SharedRenderData();

        ViewLocal<vsg::ref_ptr<ViewDependentStateEx>> viewDependentState;

        // descriptors shared by all views:
        vsg::ref_ptr<vsg::DescriptorImage> decalTextures;

        //! Rebuilds the fixed projected-texture descriptor array. Call this during
        //! application startup, before any graphics pipelines are compiled.
        void configureProjectedTextureCapacity(std::uint32_t capacity);

        //! Number of unique projected textures that can be resident concurrently.
        std::uint32_t projectedTextureCapacity() const;

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

