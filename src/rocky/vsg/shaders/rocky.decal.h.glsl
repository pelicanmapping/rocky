/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
 
// GLSL include file - decal subsystem
#include "rocky.defines.h.glsl"

// SSBO containing the output tiles that pass cull (GPU only)
layout(std140, binding = BINDING_DECAL_TILES) buffer DecalTiles
{
    DecalTile decalTiles[];
};

//SSBO containing the input decals to be culled
layout(std140, binding = BINDING_DECALS) buffer Decals
{
    Decal decals[];
};
