#pragma once

#include "r2_structs.h"
#include "r2_resources.h"

class VulkanEngine;

namespace R2 {

	void init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView);

}