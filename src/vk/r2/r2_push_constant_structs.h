#pragma once

#include "vk/vk_types.h"

namespace R2 {

	struct DrawGenPushConstants {
		VkDeviceAddress meshBuffer;
		VkDeviceAddress objectBuffer;
		VkDeviceAddress shaderObjectIndexBuffers;
		VkDeviceAddress shaderInstanceBuffers;
		VkDeviceAddress shaderIndirectBuffers;
		VkDeviceAddress objectCountBuffer;
		VkDeviceAddress drawCountBuffer;
		VkDeviceAddress perMaterialWorldExtentsBuffer;
	};

	struct DrawGenWorldExtentReadPushConstants {
		VkDeviceAddress perMaterialWorldExtentsBuffer;
		VkDeviceAddress worldExtentsBuffer;
		uint32_t materialCount;
	};

	struct MeshPrepassPushConstants {
		VkDeviceAddress surfaceBuffers;
		VkDeviceAddress materialBuffers;
	};
	
	struct LightCullingPushConstants {
		VkDeviceAddress lightBuffer;
		VkDeviceAddress lightIndexBuffer;
		//VkDeviceAddress shadowIndexBuffer;
	};

	struct LightLBVHEncodePushConstants {
		VkDeviceAddress worldExtentsBuffer;
		VkDeviceAddress lightBuffer;
		VkDeviceAddress mortonKeysInBuffer;
		VkDeviceAddress indexInBuffer;
		VkDeviceAddress nodeBuffer;
		VkDeviceAddress nodeBuildBuffer;
		uint32_t lightCount;
	};

	struct LightLBVHSortPushConstants {
		VkDeviceAddress mortonKeysInBuffer;
		VkDeviceAddress mortonKeysOutBuffer;
		VkDeviceAddress indexInBuffer;
		VkDeviceAddress indexOutBuffer;
		uint32_t lightCount;
	};

	struct LightLBVHBuildPushConstants {
		VkDeviceAddress mortonKeysInBuffer;
		VkDeviceAddress nodeBuffer;
		VkDeviceAddress leafBuffer;
		VkDeviceAddress nodeBuildBuffer;
		VkDeviceAddress leafBuildBuffer;
		uint32_t lightCount;
	};

	struct LightLBVHAABBPushConstants {
		VkDeviceAddress nodeBuffer;
		VkDeviceAddress leafBuffer;
		VkDeviceAddress nodeBuildBuffer;
		VkDeviceAddress leafBuildBuffer;
		VkDeviceAddress indexInBuffer;
		VkDeviceAddress lightBuffer;
		uint32_t lightCount;
	};

	struct GIProbeGenPushConstants {
		VkDeviceAddress probeBuffer;
		VkDeviceAddress hashBuffer;
		uint32_t hashTableMask;
		uint32_t cascadeLevel;
	};

	struct GIProbeWritePushConstants {
		VkDeviceAddress probeBuffer;
		VkDeviceAddress probeWriteBuffer;
		VkDeviceAddress ageBuffer;
		VkDeviceAddress keyBuffer;
		VkDeviceAddress probeIndexBuffer;
		VkDeviceAddress probeIndexWriteBuffer;
		VkDeviceAddress probeCountBuffer;
		uint32_t cascadeLevel;
	};

	struct GIProbeCompactPushConstants {
		VkDeviceAddress probeBuffer;
		VkDeviceAddress probeWriteBuffer;
		VkDeviceAddress ageBuffer;
		VkDeviceAddress keyBuffer;
		VkDeviceAddress probeIndexBuffer;
		VkDeviceAddress probeIndexWriteBuffer;
		VkDeviceAddress probeCountBuffer;
		VkDeviceAddress feedbackBuffer;
		VkDeviceAddress rtIndirectCommandBuffer;
		uint32_t cascadeLevel;
	};

	struct GIProbeRTPushConstants {
		VkDeviceAddress surfaceBuffers;
		VkDeviceAddress objectRTDataBuffer;
		VkDeviceAddress probeBuffer;
		VkDeviceAddress lightBuffer;
		VkDeviceAddress nodeBuffer;
		VkDeviceAddress leafBuffer;
		uint32_t lightCount;
	};

	struct GIRCEncodePushConstants {
		VkDeviceAddress h0Buffer;
		VkDeviceAddress h1Buffer;
		VkDeviceAddress h2Buffer;
		VkDeviceAddress h3Buffer;
	};

	struct GIRCAtrousPushConstants {
		int32_t iteration;
	};

	struct MeshLightingPushConstants {
		VkDeviceAddress surfaceBuffers;
		VkDeviceAddress materialBuffers;
		VkDeviceAddress lightingBuffers;
	};

	struct PostProcessingPushConstants {
		glm::vec4 hdr;
	};
}