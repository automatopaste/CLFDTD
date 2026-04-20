#pragma once

#include "r2_resources.h"

namespace DrawGen {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline drawGenPipeline;
		R2::Pipeline drawGenWorldExtentReadPipeline;
	};
}

namespace MeshPrepass {

	struct Data {
		R2::RenderPass renderPass;
	};

}

namespace LightCulling {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline lightCullingPipeline;
	};

}

namespace LightBVH {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline encodePipeline;
		R2::Pipeline sortPipeline;
		R2::Pipeline buildPipeline;
		R2::Pipeline aabbPipeline;
		R2::DynamicBufferState mortonKeysInBuffer;
		R2::DynamicBufferState mortonKeysOutBuffer;
		R2::DynamicBufferState indexInBuffer;
		R2::DynamicBufferState indexOutBuffer;
		R2::DynamicBufferState nodeBuffer;
		R2::DynamicBufferState leafBuffer;
		R2::DynamicBufferState nodeBuildBuffer;
		R2::DynamicBufferState leafBuildBuffer;
		R2::BufferState readbackBuffer;
		R2::Semaphore readbackSemaphore;
	};

}

namespace GIProbeGen {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline giProbeGenPipeline;
		R2::Pipeline giProbeWritePipeline;
		R2::Pipeline giProbeCompactPipeline;

		R2::BufferState hashBuffer;
		uint32_t hashTableTotalBucketCount;
	};

}

namespace GIProbeRT {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline giProbeRTPipeline;
		R2::ShaderBindingTable giProbeRTSBT;
	};

}

namespace GIRC {

	struct Data {
		R2::RenderPass renderPass;
		R2::Pipeline encodePipeline;
		R2::Pipeline atrousPipeline;
		R2::BufferState h0Buffer;
		R2::BufferState h1Buffer;
		R2::BufferState h2Buffer;
		R2::BufferState h3Buffer;
	};
}

namespace MeshLighting {

	struct FrameData {
		R2::BufferState lightingBuffer;
	};

	struct Data {
		R2::RenderPass renderPass;
	};
}

namespace Output {

	struct Data {
		R2::Pipeline postProcessingPipeline;
	};
}