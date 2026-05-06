#include "2d_output.h"

#include <format>

#include "imgui.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"
#include "vk/vk_images.h"

#include "../2d_basic_commander.h"

void Output::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	D2::Data& data = renderData.get_user_data<D2::Data>();

	// post processing

	DescriptorLayoutBuilder builder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	renderData.postProcessingDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorSetLayout layouts[] = { data.globalDescriptor.layout, renderData.postProcessingDescriptor.layout };
	uint32_t descriptorSetLayoutCount = sizeof(layouts) / sizeof(VkDescriptorSetLayout);

	VkPushConstantRange postProcessingPushConstantRange{};
	postProcessingPushConstantRange.offset = 0;
	postProcessingPushConstantRange.size = sizeof(D2::PostProcessingPushConstants);
	postProcessingPushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo postProcessingPipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	postProcessingPipelineLayoutCreateInfo.pPushConstantRanges = &postProcessingPushConstantRange;
	postProcessingPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	postProcessingPipelineLayoutCreateInfo.pSetLayouts = layouts;
	postProcessingPipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutCount;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &postProcessingPipelineLayoutCreateInfo, nullptr, &data.outputData.postProcessingPipeline.layout));

	VkShaderModule postProcessingShader;
	vkutil::check_compile_load("r2_post_processing.comp", engine, &postProcessingShader, engine->_compileShaders);

	ComputePipelineBuilder computePipelineBuilder{};
	computePipelineBuilder.set_shader(postProcessingShader);
	computePipelineBuilder.set_pipeline_layout(data.outputData.postProcessingPipeline.layout);
	computePipelineBuilder.build(engine->_device, data.outputData.postProcessingPipeline.pipeline);

	vkDestroyShaderModule(engine->_device, postProcessingShader, nullptr);
}

void Output::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	D2::Data& data = renderData.get_user_data<D2::Data>();

	R2::destroy_pipeline(engine, data.outputData.postProcessingPipeline);

	R2::destroy_descriptor(engine, renderData.postProcessingDescriptor);
}

void Output::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex)
{
	D2::Data& data = renderData.get_user_data<D2::Data>();
	D2::UpdateData& updateData = data.updateData;

	static bool showGa{ false };
	static float fieldScaleMult{ 500.0f };
	static float fieldScaleLimit{ 5.0f };
	if (ImGui::Begin("Simulation Output")) {
		ImGui::Checkbox("Show Dielectric", &showGa);
		ImGui::SliderFloat("Scaling Multiplier", &fieldScaleMult, 1.0f, 4000.0f);
		ImGui::SliderFloat("Scaling Limit", &fieldScaleLimit, 1.0f, 5.0f);
		ImGui::Text(std::format("Time: {}", updateData.t).c_str());
		ImGui::Text(std::format("Step: {}", updateData.step).c_str());
	}
	ImGui::End();

	vkutil::image_barrier(
		cmd,
		renderData.frameData[frameIndex].swapchainImage,
		VK_PIPELINE_STAGE_2_NONE,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_NONE,
		VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	VkDescriptorSet& postProcessingSet = renderData.postProcessingDescriptor.set(frameIndex);
	postProcessingSet = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.postProcessingDescriptor.layout);

	VkSampler sampler = engine->resolutionScale >= 1.0f ? engine->_defaultSamplerLinear : engine->_defaultSamplerNearest;

	DescriptorWriter descriptorWriter{};
	descriptorWriter.write_image(0, renderData.drawImage.image.imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorWriter.write_image(1, renderData.frameData[frameIndex].swapchainImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.write_image(2, updateData.ezImage.dst().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.write_image(3, updateData.gaImage.src().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWriter.update_set(engine->_device, postProcessingSet);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, data.outputData.postProcessingPipeline.layout, 1, 1, &postProcessingSet, 0, nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, data.outputData.postProcessingPipeline.pipeline);

	D2::PostProcessingPushConstants pushConstants{
		.data = glm::uvec4{ showGa ? 1 : 0, 0, 0, 0 },
		.scaling = glm::vec4{ fieldScaleMult, fieldScaleLimit, 0.0f, 0.0f }
	};
	vkCmdPushConstants(cmd, data.outputData.postProcessingPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(D2::PostProcessingPushConstants), &pushConstants);

	uint32_t dispatchX = static_cast<uint32_t>(std::ceil(renderData.swapchainExtent.width / 8.0f));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceil(renderData.swapchainExtent.height / 8.0f));
	vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

	// copy debug image into swapchain

	// ready for gui to render into colour attachment layout
	vkutil::image_barrier(
		cmd,
		renderData.frameData[frameIndex].swapchainImage,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	);
}
