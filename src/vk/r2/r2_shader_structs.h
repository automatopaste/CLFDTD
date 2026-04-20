#pragma once

#include "vk/vk_types.h"

namespace R2 {

	struct PBRChannel {
		glm::vec4 colourFactors;
		glm::vec4 metalRoughFactors;
		glm::vec4 emissiveFactors;
		uint32_t colour;
		uint32_t metalRough;
		uint32_t normal;
		uint32_t emissive;
	};

    struct GlobalData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewproj;
        glm::mat4 invviewproj;
        glm::mat4 invproj;
        glm::mat4 invview;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection; // w for sun power
        glm::vec4 sunlightColor;
        glm::vec4 viewPos;
        glm::uvec4 viewport; // x, y length and width
    };

    struct VertexPos {
        glm::vec3 position;
    };
    struct VertexAtt {
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 colour;
    };

    struct Weight {
        glm::vec4 joints;
        glm::vec4 weights;
    };

    typedef uint32_t Index;

    struct Mesh {
        uint32_t indexCount{ 0 };
        uint32_t firstIndex{ 0 };
        uint32_t firstVertex{ 0 };
    };

    struct alignas(16) Object {
        glm::mat4 model; // column major: [0][3] is pbr channel index, [1][3] is weight index, [2][3] is first vertex index
        glm::vec4 data0; // xyz: centre pos, w: mesh index
        glm::vec4 data1; // xyz: aabb extent, w: material index
        glm::vec4 data2; // xyz: world pos, w: spherical radius
        //glm::vec4 data3;

        inline const uint32_t mesh() const {
            return *(uint32_t*)&data0.w;
        }
        inline const uint32_t material() const {
            return *(uint32_t*)&data1.w;
        }
        inline const uint32_t pbr_channel_index() const {
            return *(uint32_t*)&model[0][3];
        }
        inline const uint32_t weight_index() const {
            return *(uint32_t*)&model[1][3];
        }
        inline const uint32_t first_vertex_index() const {
            return *(uint32_t*)&model[2][3];
        }

        inline void const set_centre(glm::vec3& centre) {
            data0.x = centre.x;
            data0.y = centre.y;
            data0.z = centre.z;
        }

        inline void const set_mesh(uint32_t& mesh) {
            data0.w = *(float*)&mesh;
        }

        inline void const set_extent(glm::vec3& extent) {
            data1.x = extent.x;
            data1.y = extent.y;
            data1.z = extent.z;
        }

        inline void const set_material_object_index(uint32_t& material) {
            data1.w = *(float*)&material;
        }

        inline void const set_world_pos(glm::vec3& worldPos) {
            data2.x = worldPos.x;
            data2.y = worldPos.y;
            data2.z = worldPos.z;
        }

        inline glm::vec3 const get_world_pos() {
            return glm::vec3{ data2.x, data2.y, data2.z };
        }

        inline void const set_radius(float radius) {
            data2.w = radius;
        }

        inline void const set_pbr_channel_index(uint32_t index) {
            model[0][3] = *(float*)&index;
        }

        inline void const set_weight_index(uint32_t index) {
            model[1][3] = *(float*)&index;
        }

        inline void const set_first_vertex_index(uint32_t index) {
            model[2][3] = *(float*)&index;
        }
    };

    struct ObjectRTData {
        uint32_t materialIndex;
        uint32_t firstIndex;
        uint32_t firstVertex;
    };

    struct InstanceData {
        glm::mat4 model;
        glm::vec4 normal0; // w: material index
        glm::vec4 normal1; // w: pbr channel index
        glm::vec4 normal2; // w: weight index
    };

    struct ObjectCountData {
        uint32_t count;
        uint32_t first;
    };

    struct PointLight {
        glm::vec4 color;
        glm::vec4 position;
        glm::vec4 data; // x = falloff radius, y = shadow index

        inline void set_falloff_radius(float radius) {
            data.x = radius;
        }
        inline void set_shadow_index(uint32_t index) {
            data.y = *(float*)&index;
        }

    };

    struct PointLightIndex {
        uint16_t index;
    };

    // calculate light space matrices in vertex shader to avoid sending 6 mat4s of memory per shadow
    struct PointShadow {
        glm::vec4 position; // xyz = pos, w = radius
    };

    struct GIProbeIndex {
        uint16_t index;
    };

    struct GIProbe {
        glm::vec3 pos;
    };

    struct GIFeedbackBuffer {
        uint32_t totalProbeCount;
    };

    struct MaterialBuffers {
        VkDeviceAddress instanceBuffer;
        VkDeviceAddress materialDataBuffer;
    };

    struct SurfaceBuffers {
        VkDeviceAddress vertexPosBuffer;
        VkDeviceAddress vertexAttBuffer;
        VkDeviceAddress indexBuffer;
        VkDeviceAddress pbrChannelBuffer;
        VkDeviceAddress weightBuffer;
        VkDeviceAddress objectBuffer;
    };
    
    struct LightingBuffers {
        VkDeviceAddress shadowmapMatrixBuffer;
        VkDeviceAddress lightBuffer;
        VkDeviceAddress lightIndexBuffer;
        VkDeviceAddress shadowcastingIndexBuffer;
        VkDeviceAddress rc0KeyBuffer;
        VkDeviceAddress rc0ProbeIndexBuffer;
    };

    struct WorldExtents {
        glm::vec3 minExtent;
        glm::vec3 maxExtent;
    };

    struct WorldExtentsBuffer {
        glm::vec3 minExtent;
        glm::vec3 maxExtent;
    };

    struct HashEntry {
        uint32_t key;
    };

    struct LBVHNode {
        uint32_t child;
        uint32_t skip;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    struct LBVHLeaf {
        uint32_t skip;
        uint32_t lightIndex;
    };

    struct LBVHNodeBuild {
        uint32_t accessCount;
        uint32_t aabbAccessCount;
        uint32_t parent;
        glm::uvec3 aabbMinEncoded;
        glm::uvec3 aabbMaxEncoded;
    };

    struct LBVHLeafBuild {
        uint32_t parent;
    };

    struct HaarCoeffs {
        glm::ivec3 ll;
        glm::ivec3 hl;
        glm::ivec3 lh;
        glm::ivec3 hh;
    };

}
