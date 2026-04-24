#pragma once

#include "vk/vk_types.h"

namespace R2 {

	struct GlobalData {
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
		glm::mat4 invviewproj;
		glm::mat4 invproj;
		glm::mat4 invview;
		glm::vec4 ambientColor;
		glm::vec4 sunlightDirection; // w for sun power
		glm::vec4 sunlightColor;
		glm::vec4 viewPos;
		glm::uvec4 viewport; // x, y length and width
	};

	struct PMLBuffers {
		VkDeviceAddress gi2Buffer;
		VkDeviceAddress gi3Buffer;
		VkDeviceAddress gj2Buffer;
		VkDeviceAddress gj3Buffer;
		VkDeviceAddress fi1Buffer;
		VkDeviceAddress fi2Buffer;
		VkDeviceAddress fi3Buffer;
		VkDeviceAddress fj1Buffer;
		VkDeviceAddress fj2Buffer;
		VkDeviceAddress fj3Buffer;
	};

}
