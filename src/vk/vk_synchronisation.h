#pragma once

#include <Volk/volk.h>

#include "vk_initialisers.h"

struct BufferSyncData {
	VkBuffer buffer;
	VkDeviceSize offset;
	VkDeviceSize size;
};

namespace vkutil {
	// bible -> https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect);
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount);
	void image_barrier(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect);
	void image_barrier(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount);
	void image_barrier_mipchain_generate(VkCommandBuffer cmd, VkImage image, VkFilter filter, VkExtent2D imageSize, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount);
	void image_barrier_count(VkCommandBuffer cmd, VkImage* images, uint32_t count, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect);
	void memory_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
	void buffer_barrier(VkCommandBuffer cmd, VkBuffer buffer, uint32_t queue, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
	void buffer_barrier_count(VkCommandBuffer cmd, BufferSyncData* buffers, uint32_t count, uint32_t queue, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
	void debug_full_memory_barrier(VkCommandBuffer cmd, const char* text);
}