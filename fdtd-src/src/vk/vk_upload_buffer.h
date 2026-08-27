#pragma once

#include "vk_types.h"

class VulkanEngine;

/*
* Grows to fit static resize, never shrinks
*/
class UploadBuffer {
public:

    VkBuffer hostBuffer{ 0 };
    VmaAllocation hostAllocation{};
    VmaAllocationInfo hostInfo{};
    void* hostData{ nullptr };

    VkBuffer gpuBuffer{ 0 };
    VmaAllocation gpuAllocation{};
    VmaAllocationInfo gpuInfo{};
    VkDeviceAddress address{ 0 };

    uint32_t contentSize{ 0 };
    uint32_t bufferSize{ 0 };

	void reset();
	void resize(uint32_t newSize, VulkanEngine* engine);
	void* append(uint32_t appended);
    void upload(VkCommandBuffer cmd, VulkanEngine* engine) const;
    void deallocate(VulkanEngine* engine);

};