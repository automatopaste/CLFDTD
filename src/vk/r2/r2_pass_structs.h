#pragma once

#include "r2_resources.h"
#include <array>

namespace Update {

	struct Data {
		R2::Pipeline initPipeline;
		R2::Pipeline ezUpdatePipeline;
		R2::Pipeline hxyUpdatePipeline;

		R2::PingPongImage ezImage;
		R2::PingPongImage dzImage;
		R2::PingPongImage hxImage;
		R2::PingPongImage hyImage;
		R2::PingPongImage gaImage;
		R2::PingPongImage ihxImage;
		R2::PingPongImage ihyImage;

		R2::BufferState pmlBuffers;
		R2::BufferState gi2Buffer;
		R2::BufferState gi3Buffer;
		R2::BufferState gj2Buffer;
		R2::BufferState gj3Buffer;
		R2::BufferState fi1Buffer;
		R2::BufferState fi2Buffer;
		R2::BufferState fi3Buffer;
		R2::BufferState fj1Buffer;
		R2::BufferState fj2Buffer;
		R2::BufferState fj3Buffer;

		R2::BufferState sampleBuffer;

		// program variables
		float ddx;
		float dt;
		float t;
		uint32_t step;
		int32_t npml;
		uint32_t x;
		uint32_t y;
		uint32_t z;
		bool initialise;
		bool fileWrite;
	};

}

namespace Output {

	struct Data {
		R2::Pipeline postProcessingPipeline;
	};
}