#include "r2_update.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

#include "vk/r2/r2_config.h"
#include "vk/r2/r2_shader_structs.h"
#include "vk/r2/r2_push_constant_structs.h"

#include "imgui.h"

#include "glm/glm.hpp"

void Update::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	Update::Data& data = renderData.updateData;

	data.ddx = 0.01f;
	data.dt = data.ddx / 6e8f;
	data.t = 0.0f;
	data.step = 0;
	

	for (uint32_t i = 0; i < 2; i++) {
		Update::FrameData& frameData = renderData.updateData.frameData[i];
		
		VkExtent3D size = { 800, 800, 1 };
		VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		R2::create_image(engine, &frameData.ezFieldImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
		R2::create_image(engine, &frameData.hxFieldImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
		R2::create_image(engine, &frameData.hyFieldImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	renderData.updateDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorSetLayout layouts[] = { renderData.globalDescriptor.layout, renderData.updateDescriptor.layout };
	uint32_t descriptorSetLayoutCount = sizeof(layouts) / sizeof(VkDescriptorSetLayout);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(R2::UpdatePushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = layouts;
	pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutCount;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &pipelineLayoutCreateInfo, nullptr, &renderData.updateData.updatePipeline.layout));

	VkShaderModule shader;
	vkutil::check_compile_load("r2_update.comp", engine, &shader, engine->_compileShaders);

	ComputePipelineBuilder computePipelineBuilder{};
	computePipelineBuilder.set_shader(shader);
	computePipelineBuilder.set_pipeline_layout(renderData.updateData.updatePipeline.layout);
	computePipelineBuilder.build(engine->_device, renderData.updateData.updatePipeline.pipeline);

	vkDestroyShaderModule(engine->_device, shader, nullptr);
}

void Update::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	for (uint32_t i = 0; i < 2; i++) {
		Update::FrameData& frameData = renderData.updateData.frameData[i];
		R2::destroy_image(engine, frameData.ezFieldImage);
		R2::destroy_image(engine, frameData.hxFieldImage);
		R2::destroy_image(engine, frameData.hyFieldImage);
	}

	R2::destroy_descriptor(engine, renderData.updateDescriptor);
}

void Update::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex)
{
	Update::Data& data = renderData.updateData;
	Update::FrameData& frameData0 = data.frameData[data.step % 2];
	Update::FrameData& frameData1 = data.frameData[(data.step + 1) % 2];

	VkDescriptorSet& updateDescriptor = renderData.updateDescriptor.set(frameIndex);
	updateDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.updateDescriptor.layout);

	DescriptorWriter descriptorWriter{};

	descriptorWriter.write_image(0, frameData0.ezFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.write_image(1, frameData1.ezFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	descriptorWriter.write_image(2, frameData0.hxFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.write_image(3, frameData1.hxFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	descriptorWriter.write_image(4, frameData0.hyFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.write_image(5, frameData1.hyFieldImage.image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	descriptorWriter.update_set(engine->_device, updateDescriptor);

	VkDescriptorSet sets[]{ renderData.globalDescriptor.set(frameIndex), updateDescriptor };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderData.updateData.updatePipeline.layout, 0, sizeof(sets) / sizeof(VkDescriptorSet), sets, 0, nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderData.updateData.updatePipeline.pipeline);

	R2::UpdatePushConstants pushConstants{
		.data = glm::vec4{ 0.0f, 0.0f, 0.0f, 0.0f }
	};
	vkCmdPushConstants(cmd, renderData.updateData.updatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(R2::UpdatePushConstants), &pushConstants);

	uint32_t dispatchX = static_cast<uint32_t>(std::ceil(renderData.swapchainExtent.width / 8.0f));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceil(renderData.swapchainExtent.height / 8.0f));
	vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

}
