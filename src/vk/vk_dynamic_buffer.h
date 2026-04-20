#pragma once

#include "vk_types.h"

class VulkanEngine;

/*
* Dynamically-sized upload buffer, requires host sync with a semaphore
*/
struct DynamicUploadBuffer {
    VkBuffer hostBuffer{ 0 };
    uint32_t hostBufferSize{ 0 };
    VmaAllocation hostAllocation{};
    VmaAllocationInfo hostInfo{};
    void* hostData{ nullptr };

    VkBuffer gpuBuffer{ 0 };
    uint32_t gpuBufferSize{ 0 };
    VmaAllocation gpuAllocation{};
    VmaAllocationInfo gpuInfo{};
    VkDeviceAddress address{ 0 };

    uint32_t contentSize{ 0 };
    bool markReset{ false };

    void reset();
    // returns pointer to start of buffer
    void* fit(uint32_t size, VulkanEngine* engine);
    // returns pointer to start of appended region
    void* append(uint32_t appended, VulkanEngine* engine);
    void flush(VulkanEngine* engine) const;
    void upload(VkCommandBuffer cmd, VulkanEngine* engine) const;
    void deallocate(VulkanEngine* engine);
    void reallocate(uint32_t size, VulkanEngine* engine);

};

struct DynamicBuffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
    VkDeviceAddress address;
    uint32_t capacity{ 0 };
    VkBufferUsageFlags usage{ 0 };

    void fit(uint32_t size, VulkanEngine* engine);
    void alloc(uint32_t size, VulkanEngine* engine, VkBufferUsageFlags usage);
    void destroy(VulkanEngine* engine);

};