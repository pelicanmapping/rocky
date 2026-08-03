// include from either GLSL or C++
#ifndef ROCKY_DEFINES
#define ROCKY_DEFINES

#define ROCKY_HAS_DECALS


// transient/local bindings (in descriptor set 0):
#define DESCRIPTOR_SET_LOCAL                 0

#define BINDING_TERRAIN_COLOR                1
#define BINDING_TERRAIN_ELEVATION            2
#define BINDING_TERRAIN_TILE                 3


// view dependent state (in descriptor set 1):
#define DESCRIPTOR_SET_VDS                   1

#define BINDING_VDS_VSG_LIGHTS               0
#define BINDING_VDS_VSG_VIEWPORTS            1
#define BINDING_VDS_RENDER_PARAMS           10
#define BINDING_VDS_FRUSTUM_GRID_PARAMS     11
#define BINDING_VDS_FRUSTUMS                12
#define BINDING_VDS_DECALS                  13
#define BINDING_VDS_DECAL_TILES             14

// map global state (in descriptor set 2):
#define DESCRIPTOR_SET_GLOBAL                2

//#define BINDING_MAP_SETTINGS                 1
#define BINDING_TERRAIN_SETTINGS             1
#define BINDING_DECAL_TEXTURES               2


// configuration and limits:
#define FRUSTUM_GRID_TILE_SIZE_PIXELS       16
#define FRUSTUM_GRID_TILES_PER_THREAD_GROUP 16
#define MAX_DECALS_PER_TILE                  7
#define MAX_NUM_DECAL_TEXTURES              64


#ifdef __cplusplus

#define TYPE_VDS_RENDER_PARAMS        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
#define TYPE_VDS_FRUSTUM_GRID_PARAMS  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
#define TYPE_VDS_FRUSTUMS             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER

#define TYPE_VDS_DECALS               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
#define TYPE_VDS_DECAL_TILES          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
#define TYPE_DECAL_TEXTURES           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER

//#define TYPE_MAP_SETTINGS             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
#define TYPE_TERRAIN_SETTINGS         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
#define TYPE_TERRAIN_TILE             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

#endif

#endif // ROCKY_DEFINES

