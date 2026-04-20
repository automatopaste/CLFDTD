#pragma once

#include <Volk/volk.h>

#include "vk_initialisers.h"

namespace vkutil {

	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, VkImageAspectFlags aspect, VkFilter filter = VK_FILTER_NEAREST, uint32_t srcMip = 0);
	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, VkImageLayout finalLayout, VkImageAspectFlags aspectFlags, bool transition);
	void generate_mipmap_level(VkCommandBuffer cmd, VkImage image, VkExtent2D srcSize, VkExtent2D dstSize, uint32_t mipLevel);
	uint32_t max_mip_levels(VkExtent3D size);
	uint32_t max_mip_levels(VkExtent2D size);

}