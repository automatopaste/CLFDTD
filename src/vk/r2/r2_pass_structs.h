#pragma once

#include "r2_resources.h"
#include <array>

namespace Update {

	struct FrameData {
		R2::ImageState ezFieldImage;
		R2::ImageState hxFieldImage;
		R2::ImageState hyFieldImage;
	};

	struct Data {
		R2::Pipeline updatePipeline;
		std::array<FrameData, 2> frameData;

		// program variables
		float ddx;
		float dt;
		float t;
		uint32_t step;
	};

}

namespace Output {

	struct Data {
		R2::Pipeline postProcessingPipeline;
	};
}