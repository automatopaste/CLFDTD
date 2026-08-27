#pragma once

#include "vk/r2/r2_structs.h"
#include "../3d_linear_commander.h"

class VulkanEngine;

namespace L3DUpdate {

	void init(R2::RenderData& renderData, VulkanEngine* engine);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, L3D::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex);

}