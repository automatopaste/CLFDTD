#pragma once

#include "vk/vk_types.h"

namespace R2 {

	struct UpdatePushConstants {
		glm::vec4 data;
	};

	struct PostProcessingPushConstants {
		glm::vec4 hdr;
	};
}