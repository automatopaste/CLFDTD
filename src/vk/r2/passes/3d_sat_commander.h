#pragma once

#include <thread>
#include <array>
#include <shared_mutex>
#include "../r2_resources.h"
#include "../r2_structs.h"

class VulkanEngine;

namespace L3Dsat {

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

	enum EFieldUpdateShader {
		PROPAGATION,
		GAIN,
		CAVITY,
		CAVITY3D,
		SEL
	};

	struct UpdateData {
		std::array<DescriptorAllocatorGrowable, FRAME_OVERLAP> descriptorAllocators;
		R2::Descriptor initDescriptor;
		R2::Descriptor eDescriptor;
		R2::Descriptor hDescriptor;

		R2::Pipeline initPipeline;
		R2::Pipeline eUpdatePipeline;
		R2::Pipeline hUpdatePipeline;

		R2::PingPongImage eImage;
		R2::PingPongImage fImage;
		R2::PingPongImage jImage;
		R2::PingPongImage hImage;
		R2::PingPongImage ePeakImage;

		// pml buffers
		R2::BufferState ABuffer;
		R2::BufferState BBuffer;
		R2::PingPongImage psiEXImage;
		R2::PingPongImage psiEYImage;
		R2::PingPongImage psiEZImage;
		R2::PingPongImage psiHXImage;
		R2::PingPongImage psiHYImage;
		R2::PingPongImage psiHZImage;

		R2::BufferState sampleBuffer0;
		R2::BufferState sampleBuffer1;
		R2::BufferState sampleBuffer2;
		R2::BufferState sampleBuffer3;
		R2::BufferState sampleBuffer4;

		// program variables
		float dx;
		float dt;
		float t;
		float c;
		float timeUnit;
		float lengthUnit;
		uint32_t step;
		uint32_t maxStep;
		int32_t npml;
		uint32_t x;
		uint32_t y;
		uint32_t z;
		bool initialise;
		bool fileWrite;
		int zRead;
		float sourceOmega0;
		EFieldUpdateShader updateShader;
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
		glm::vec4 simData2;
		glm::ivec4 simDataInt;
		VkDeviceAddress ABuffer;
		VkDeviceAddress BBuffer;
		VkDeviceAddress sampleBuffer0;
		VkDeviceAddress sampleBuffer1;
		VkDeviceAddress sampleBuffer2;
		VkDeviceAddress sampleBuffer3;
		VkDeviceAddress sampleBuffer4;
	};

	struct PostProcessingPushConstants {
		glm::uvec4 data;
		glm::vec4 scaling;
	};

	void init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent);

	void cleanup(R2::RenderData& renderData, VulkanEngine* engine);

	void render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView);

}