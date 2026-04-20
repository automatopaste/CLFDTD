#pragma once

#include "vk/vk_types.h"
#include "vk/vk_dynamic_buffer.h"

class VulkanEngine;

namespace R2 {

	struct ImageState {

		// image

		AllocatedImage image;
		uint32_t allocationIndex{ 0 };

		// state

		VkImageAspectFlags aspect{ VK_IMAGE_ASPECT_NONE };
		VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };
	};

	struct ImageResource {
		ImageState* image;
		VkImageLayout dstLayout;
		VkPipelineStageFlags2 srcStage;
		VkPipelineStageFlags2 dstStage;
		VkAccessFlags2 srcAccess;
		VkAccessFlags2 dstAccess;
		uint32_t mipLevels{ 0 };
		VkFilter filter{ VK_FILTER_NEAREST };
	};

	void create_image(VulkanEngine* engine, ImageState* dest, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageCreateFlags create, VkImageAspectFlags aspect);
	void destroy_image(VulkanEngine* engine, ImageState& image);

	void image_barrier(ImageResource* image, VkCommandBuffer cmd);

	struct BufferState {

		// buffer

		VkBuffer buffer{ 0 };
		VkDeviceAddress address{ 0 };
		uint32_t size{ 0 };
		VmaAllocation allocation{};
		VmaAllocationInfo info{};
		uint32_t allocationIndex{ 0 };

		// state

		VkAccessFlags2 currAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 currStage{ VK_PIPELINE_STAGE_2_NONE };
	};

	void create_buffer(VulkanEngine* engine, BufferState* dest, size_t size, VkBufferUsageFlags usage, bool mapped, uint32_t alignment = 16);
	void destroy_buffer(VulkanEngine* engine, BufferState& buffer);

	struct DynamicUploadBufferState {

		// buffer

		DynamicUploadBuffer buffer{};
		uint32_t allocationIndex{ 0 };

		// state

		VkAccessFlags2 currAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 currStage{ VK_PIPELINE_STAGE_2_NONE };
	};

	void create_buffer(VulkanEngine* engine, DynamicUploadBufferState* dest);
	void destroy_buffer(VulkanEngine* engine, DynamicUploadBufferState& buffer);

	struct DynamicBufferState {
		DynamicBuffer buffer{};
		uint32_t allocationIndex{ 0 };
		VkAccessFlags2 currAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 currStage{ VK_PIPELINE_STAGE_2_NONE };
	};

	void create_buffer(VulkanEngine* engine, DynamicBufferState* dest, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	void destroy_buffer(VulkanEngine* engine, DynamicBufferState& buffer);

	struct Descriptor {
		VkDescriptorSetLayout layout{ nullptr };
		std::array<VkDescriptorSet, FRAME_OVERLAP> sets;

		inline VkDescriptorSet& set(uint32_t frameIndex) {
			return sets[frameIndex];
		}
	};

	void destroy_descriptor(VulkanEngine* engine, Descriptor& descriptor);

	struct Pipeline {
		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	void destroy_pipeline(VulkanEngine* engine, Pipeline& pipeline);

	struct Semaphore {
		VkSemaphore semaphore;
		uint64_t value{ 0 };
	};

	void create_semaphore(VulkanEngine* engine, Semaphore* dest, uint64_t initialValue = 0);
	void signal_semaphore(VulkanEngine* engine, Semaphore& semaphore, VkPipelineStageFlags2 stage, uint64_t value);
	void wait_semaphore(VulkanEngine* engine, Semaphore& semaphore, uint64_t value);
	void destroy_semaphore(VulkanEngine* engine, Semaphore& semaphore);

	struct RenderPass {
		VkAccessFlags2 srcAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 srcStage{ VK_PIPELINE_STAGE_2_NONE };
		VkAccessFlags2 dstAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 dstStage{ VK_PIPELINE_STAGE_2_NONE };
		
		std::vector<ImageResource> imageResources;

		// from dependencies

		VkAccessFlags2 dependentAccess{ VK_ACCESS_2_NONE };
		VkPipelineStageFlags2 dependentStage{ VK_PIPELINE_STAGE_2_NONE };
	};

	void pass_init(RenderPass* dest, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 srcStage, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstStage);
	void pass_add_dependency(RenderPass* src, RenderPass* dst);
	void set_image_layout_undefined(ImageState* image);
	void pass_add_image_transition(RenderPass* src, ImageState* image, VkImageLayout layout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, uint32_t mipLevels, VkFilter filter = VK_FILTER_NEAREST);
	void pass_cmd_barrier(RenderPass* renderPass, VkCommandBuffer cmd);

	struct GroupTypeData {
		VkStridedDeviceAddressRegionKHR strided{ 0 };
		uint32_t dataSize{ 0 };
		std::vector<uint32_t> groupIndices;
	};

	struct ShaderBindingTable {
		std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> groupCreateInfos;
		GroupTypeData raygenData;
		GroupTypeData missData;
		GroupTypeData hitData;
		GroupTypeData callableData;

		BufferState buffer;

		// currently only supports one shader of each type
		void push_group(VkShaderModule shader, VkShaderStageFlagBits stage);

		VkPipeline build_pipeline(VulkanEngine* engine, VkPipelineLayout layout, uint32_t maxRecursionDepth);
		void build_table(VulkanEngine* engine, VkPipeline pipeline);
	};

	void destroy_sbt(VulkanEngine* engine, ShaderBindingTable& sbt);

	struct ComputeBuilder {
		VulkanEngine* engine{ nullptr };
		VkPushConstantRange pushConstantRange{};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		VkShaderModule shaderModule{};
		std::vector<uint8_t> specConstantData;
		std::vector<VkSpecializationMapEntry> specConstants;
		uint32_t specConstantOffset{ 0 };
		VkPipelineShaderStageCreateFlags createFlags{ 0 };
		void descriptors(VkDescriptorSetLayout* layouts, uint32_t count);
		void push_constants(size_t size, VkShaderStageFlags stage);
		void spec_const(uint32_t value, size_t size);
		void clear_spec_const();
		void set_shader_flags(VkPipelineShaderStageCreateFlags flags);
		void shader(const char* string);
		void build(Pipeline* dest);
	};
	
	ComputeBuilder compute_builder(VulkanEngine* engine);

	void debug_name_begin(VkCommandBuffer cmd, const char* name);
	void debug_name_end(VkCommandBuffer cmd);
	
}