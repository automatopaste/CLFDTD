#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_clustered : require
#extension GL_KHR_shader_subgroup_vote : require

#define MAX_MESH_COUNT 2048
#define MAX_VERTEX_COUNT 1 << 24
#define MAX_OBJECT_COUNT 32768
#define MAX_SHADER_OBJECTS_COUNT 8192
#define MAX_TEXTURE_COUNT 1024
#define MAX_SHADOWMAP_COUNT 16
#define MAX_POINT_LIGHT_COUNT 1024
#define MAX_MATERIAL_COUNT 256
#define MAX_WEIGHT_COUNT 2056
#define MAX_INVERSE_BIND_MATRIX_COUNT 1024
#define MAX_LIGHTS_PER_TILE 128