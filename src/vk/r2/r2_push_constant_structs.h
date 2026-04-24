#pragma once

#include "vk/vk_types.h"

#include "glm/glm.hpp"

namespace R2 {

	struct UpdatePushConstants {
		glm::uvec4 dimensions;
		glm::vec4 simData;
		glm::ivec4 simDataInt;
		VkDeviceAddress pmlBuffers;
		VkDeviceAddress sampleBuffer;
	};

	struct PostProcessingPushConstants {
		glm::uvec4 data;
		glm::vec4 scaling;
	};
}