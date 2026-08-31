#pragma once

#include "vk/r2/r2_structs.h"
#include "../3d_sat_commander.h"

class VulkanEngine;

namespace L3DsatUpdate {

	void init(R2::RenderData& renderData, VulkanEngine* engine);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, L3Dsat::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex);

}