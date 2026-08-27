#pragma once

#include "vk/r2/r2_structs.h"
#include "../2d_dispersion_commander.h"

class VulkanEngine;

namespace D2DispUpdate {

	void init(R2::RenderData& renderData, VulkanEngine* engine);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, D2Disp::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex);

}