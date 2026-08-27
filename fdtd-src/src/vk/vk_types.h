#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

//#define LM_MEMORY_ALLOC_LOG
#include <vma/vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

constexpr uint32_t FRAME_OVERLAP = 2;
constexpr uint32_t BUFFER_ALIGNMENT = 16;

#define VK_CHECK(x)                                                         \
    do {                                                                    \
        VkResult err = x;                                                   \
        if (err) {                                                          \
             fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                        \
        }                                                                   \
    } while (0)

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call functors
        }

        deletors.clear();
    }
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    uint32_t mipLevels{ 1 };
    uint32_t allocationIndex{ 0 };

    inline VkExtent2D extent() {
        return VkExtent2D{ .width = imageExtent.width, .height = imageExtent.height };
    }
};

struct AllocatedBuffer {
    VkBuffer buffer{ 0 };
    VkDeviceAddress address{ 0 };
    uint32_t size{ 0 };
    uint32_t allocSize{ 0 };
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
    uint32_t allocationIndex{ 0 };

    inline const VkDeviceAddress addr_offset(int frameIndex) const {
        assert(frameIndex * size < allocSize);
        return address + frameIndex * size;
    }

    inline void* map_offset(int frameIndex) const {
        return (void*)((char*)info.pMappedData + (size * frameIndex));
    }
};

struct MeshData {
    uint32_t meshID{ 0 };
    uint32_t firstIndex{ 0 };
    uint32_t indexCount{ 0 };
    uint32_t firstVertex{ 0 };
    uint32_t vertexCount{ 0 };
};

// lighting
struct PointLight {
    glm::vec3 position;
    glm::vec3 colour;

    float constant;
    float linear;
    float quadratic;
};

// structs that have to directly match shader structs in memory layout
namespace gpu {

    struct DrawPushConstants {
        glm::mat4 worldMatrix;
        VkDeviceAddress vertexBuffer;
    };

    struct PostProcessingComputePushConstants {
        glm::vec4 hdr; // x is gamma
        VkDeviceAddress meshBuffer;
    };

    // lighting

    constexpr int FLAG_SHADOWCASTING = 1;
    constexpr int FLAG_POINT_LIGHT = 2;
    constexpr int FLAG_SPOTLIGHT = 3;

    struct SSAOData {
        glm::vec4 data;
        glm::vec4 samples[64];
    };

}
