#pragma once

#include "vk/r2/r2_structs.h"

class VulkanEngine;

namespace L3DsatReset {

	void init(R2::RenderData& renderData, VulkanEngine* engine);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex);

}