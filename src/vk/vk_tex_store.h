#pragma once

#include <unordered_map>
#include <vector>

#include "vk_types.h"

class VulkanEngine;

class TextureStore {
public:

	std::vector<AllocatedImage> textures;
	uint32_t loaded{ 0 };

	// marks if texture list has changed
	bool needsDescriptorRebuild{ true };
	
	void init(VulkanEngine* engine);

	void cleanup();

	uint32_t add_texture(AllocatedImage& image, VkImageLayout layout);

	void create_empty(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t count, VkImageCreateFlags create = 0);
	void create_empty_attachments(VkRenderingAttachmentInfo& attachmentInfo, VkClearValue& clearValue, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t count, VkImageCreateFlags create = 0);

	void clear(bool deallocateImages);

	void transition_all(VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageAspectFlags aspect);

	void write_descriptor(VkDescriptorSet set);

	uint32_t get_count();
	VkImageView* get_views();
	VkImageLayout* get_layouts();
	VkRenderingAttachmentInfo* get_attachments();

private:

	std::vector<VkImageView> imageViews;
	std::vector<VkImageLayout> imageLayouts;
	std::vector<VkRenderingAttachmentInfo> imageAttachments;

	VulkanEngine* _engine;
};