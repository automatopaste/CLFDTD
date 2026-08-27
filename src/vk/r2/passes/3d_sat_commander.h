#pragma once

#include <thread>
#include <array>
#include <shared_mutex>
#include "../r2_resources.h"
#include "../r2_structs.h"

class VulkanEngine;

namespace L3D {

	struct GlobalDataBuffer {
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

	struct GlobalData {
		std::array<R2::BufferState, FRAME_OVERLAP> globalBuffers;
	};

	struct UpdateData {
		std::array<DescriptorAllocatorGrowable, FRAME_OVERLAP> descriptorAllocators;
		R2::Descriptor initDescriptor;
		R2::Descriptor eDescriptor;
		R2::Descriptor hDescriptor;

		R2::Pipeline initPipeline;
		R2::Pipeline eUpdatePipeline;
		R2::Pipeline hUpdatePipeline;

		R2::PingPongImage gaImage;
		R2::PingPongImage eImage;
		R2::PingPongImage dImage;
		R2::PingPongImage hImage;

		/*R2::BufferState pmlBuffers;
		R2::BufferState gi2Buffer;
		R2::BufferState gi3Buffer;
		R2::BufferState gj2Buffer;
		R2::BufferState gj3Buffer;
		R2::BufferState fi1Buffer;
		R2::BufferState fi2Buffer;
		R2::BufferState fi3Buffer;
		R2::BufferState fj1Buffer;
		R2::BufferState fj2Buffer;
		R2::BufferState fj3Buffer;*/

		R2::BufferState sampleBuffer;

		// program variables
		float ddx;
		float dt;
		float t;
		uint32_t step;
		uint32_t maxStep;
		int32_t npml;
		uint32_t x;
		uint32_t y;
		uint32_t z;
		bool initialise;
		bool fileWrite;
	};

	struct OutputData {
		R2::Pipeline postProcessingPipeline;
	};

	struct Data {
		std::thread simThread;
		std::shared_mutex simThreadMutex{};
		std::atomic_bool simExitFlag{ false };
		std::atomic_bool simReadFlag{ false };

		R2::Descriptor globalDescriptor;

		// pass-specific data

		R2::RenderPass resetPass;
		R2::RenderPass outputPass;

		GlobalData globalData;
		UpdateData updateData;
		OutputData outputData;
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

	struct UpdatePushConstants {
		glm::uvec4 dimensions;
		glm::vec4 simData;
		glm::ivec4 simDataInt;
		// VkDeviceAddress pmlBuffers;
		VkDeviceAddress sampleBuffer;
	};

	struct PostProcessingPushConstants {
		glm::uvec4 data;
		glm::vec4 scaling;
	};

	void init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView);

}