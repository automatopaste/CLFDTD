#include "3d_update.h"

#include <format>

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"
#include "vk/vk_images.h"

#include "vk/r2/r2_config.h"

#include "imgui.h"

#include "glm/glm.hpp"

void L3DsatUpdate::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

	L3Dsat::UpdateData& updateData = data.updateData;

	updateData.updateShader = L3Dsat::CAVITY;

	switch (updateData.updateShader) {
		case L3Dsat::GAIN:
		{
			constexpr float c0 = 299792458.0f;
			constexpr float eps0 = 8.8541878128e-12f;
			constexpr float dx0 = 6.2e-10f;
			updateData.lengthUnit = dx0; // Normalised simulation length unit
			updateData.timeUnit = updateData.lengthUnit / c0; // Derived simulation time unit (L / c)
			constexpr float courant = 0.5f;
			updateData.dx = 1.0f;
			updateData.c = 1.0f;
			updateData.dt = courant * updateData.dx / updateData.c;
			break;
		}
		case L3Dsat::CAVITY:
		{
			constexpr float c0 = 299792458.0f;
			constexpr float eps0 = 8.8541878128e-12f;
			constexpr float dx0 = 12.4e-6 / 600.0f; // 600 cell simulation of 12.4um GaAs crystal
			updateData.lengthUnit = dx0; // Normalised simulation length unit
			updateData.timeUnit = updateData.lengthUnit / c0; // Derived simulation time unit (L / c)
			constexpr float courant = 0.5f;
			updateData.dx = 1.0f;
			updateData.c = 1.0f;
			updateData.dt = courant * updateData.dx / updateData.c;
			break;
		}
	}

	updateData.t = 0.0f;
	updateData.step = 0;
	updateData.npml = 300;
	updateData.x = 1300;
	updateData.y = 8;
	updateData.z = 8;
	updateData.initialise = true;
	updateData.fileWrite = true;
	updateData.maxStep = 5000;
	updateData.zRead = updateData.z / 2;

	size_t sizeX = updateData.x * sizeof(float);
	size_t sizeY = updateData.y * sizeof(float);
	size_t sizeZ = updateData.z * sizeof(float);

	R2::create_buffer(engine, &updateData.ABuffer, sizeof(float) * updateData.npml, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.BBuffer, sizeof(float) * updateData.npml, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false);

	size_t sampleBufferSize = updateData.maxStep * sizeof(glm::vec3);
	R2::create_buffer(engine, &updateData.sampleBuffer0, sampleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
	R2::create_buffer(engine, &updateData.sampleBuffer1, sampleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);

	VkExtent3D size = { updateData.x, updateData.y, updateData.z };
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	R2::create_image(engine, &updateData.eImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.fImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.jImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.hImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.ePeakImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	VkExtent3D psiXSize = { updateData.npml * 2, updateData.y, updateData.z };
	R2::create_image(engine, &updateData.psiEXImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.psiHXImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	VkExtent3D psiYSize = { updateData.x, updateData.npml * 2, updateData.z };
	R2::create_image(engine, &updateData.psiEYImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.psiHYImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	VkExtent3D psiZSize = { updateData.x, updateData.y, updateData.npml * 2 };
	R2::create_image(engine, &updateData.psiEZImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.psiHZImage, size, VK_FORMAT_R32G32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkImage initImageTransitions[]{
			updateData.eImage.src().image,
			updateData.eImage.dst().image,
			updateData.fImage.src().image,
			updateData.fImage.dst().image,
			updateData.jImage.src().image,
			updateData.jImage.dst().image,
			updateData.hImage.src().image,
			updateData.hImage.dst().image,
			updateData.ePeakImage.src().image,
			updateData.ePeakImage.dst().image,
			updateData.psiEXImage.src().image,
			updateData.psiEXImage.dst().image,
			updateData.psiEYImage.src().image,
			updateData.psiEYImage.dst().image,
			updateData.psiEZImage.src().image,
			updateData.psiEZImage.dst().image,
			updateData.psiHXImage.src().image,
			updateData.psiHXImage.dst().image,
			updateData.psiHYImage.src().image,
			updateData.psiHYImage.dst().image,
			updateData.psiHZImage.src().image,
			updateData.psiHZImage.dst().image
		};
		for (uint32_t i = 0; i < sizeof(initImageTransitions) / sizeof(VkImage); i++) {
			vkutil::image_barrier(cmd,
				initImageTransitions[i],
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_2_NONE,
				VK_ACCESS_2_NONE,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_ASPECT_COLOR_BIT
			);
		}
	});

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	for (int i = 0; i < 11; i++) {
		builder.add_binding(i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	}
	updateData.initDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	for (int i = 0; i < 15; i++) {
		builder.add_binding(i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	}
	updateData.eDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	for (int i = 0; i < 9; i++) {
		builder.add_binding(i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	}
	updateData.hDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(L3Dsat::UpdatePushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayout initLayouts[] = { data.globalDescriptor.layout, updateData.initDescriptor.layout };
	uint32_t initLayoutCount = sizeof(initLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo initLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	initLayoutInfo.pPushConstantRanges = &pushConstantRange;
	initLayoutInfo.pushConstantRangeCount = 1;
	initLayoutInfo.pSetLayouts = initLayouts;
	initLayoutInfo.setLayoutCount = initLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &initLayoutInfo, nullptr, &data.updateData.initPipeline.layout));

	VkDescriptorSetLayout eLayouts[] = { data.globalDescriptor.layout, updateData.eDescriptor.layout };
	uint32_t eLayoutCount = sizeof(eLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo eLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	eLayoutInfo.pPushConstantRanges = &pushConstantRange;
	eLayoutInfo.pushConstantRangeCount = 1;
	eLayoutInfo.pSetLayouts = eLayouts;
	eLayoutInfo.setLayoutCount = eLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &eLayoutInfo, nullptr, &data.updateData.eUpdatePipeline.layout));

	VkDescriptorSetLayout hLayouts[] = { data.globalDescriptor.layout, updateData.hDescriptor.layout };
	uint32_t hLayoutCount = sizeof(hLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo hLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	hLayoutInfo.pPushConstantRanges = &pushConstantRange;
	hLayoutInfo.pushConstantRangeCount = 1;
	hLayoutInfo.pSetLayouts = hLayouts;
	hLayoutInfo.setLayoutCount = hLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &hLayoutInfo, nullptr, &data.updateData.hUpdatePipeline.layout));

	const char* eUpdateShader;
	switch (updateData.updateShader) {
	case L3Dsat::GAIN:
		eUpdateShader = "3d_sat/3d_e_update_gain.comp";
		break;
	case L3Dsat::CAVITY:
		eUpdateShader = "3d_sat/3d_e_update_cavity.comp";
		break;
	}

	VkShaderModule initShader;
	VkShaderModule eShader;
	VkShaderModule hShader;
	vkutil::check_compile_load("3d_sat/3d_initialise.comp", engine, &initShader, engine->_compileShaders);
	vkutil::check_compile_load(eUpdateShader, engine, &eShader, engine->_compileShaders);
	vkutil::check_compile_load("3d_sat/3d_h_update.comp", engine, &hShader, engine->_compileShaders);

	ComputePipelineBuilder computePipelineBuilder;

	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(initShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.initPipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.initPipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(eShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.eUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.eUpdatePipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(hShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.hUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.hUpdatePipeline.pipeline);

	vkDestroyShaderModule(engine->_device, initShader, nullptr);
	vkDestroyShaderModule(engine->_device, eShader, nullptr);
	vkDestroyShaderModule(engine->_device, hShader, nullptr);
}

void L3DsatUpdate::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();
	L3Dsat::UpdateData& updateData = data.updateData;

	R2::destroy_buffer(engine, updateData.ABuffer);
	R2::destroy_buffer(engine, updateData.BBuffer);

	R2::destroy_buffer(engine, updateData.sampleBuffer0);
	R2::destroy_buffer(engine, updateData.sampleBuffer1);

	R2::destroy_image(engine, updateData.eImage);
	R2::destroy_image(engine, updateData.fImage);
	R2::destroy_image(engine, updateData.jImage);
	R2::destroy_image(engine, updateData.hImage);
	R2::destroy_image(engine, updateData.ePeakImage);

	R2::destroy_image(engine, updateData.psiEXImage);
	R2::destroy_image(engine, updateData.psiEYImage);
	R2::destroy_image(engine, updateData.psiEZImage);
	R2::destroy_image(engine, updateData.psiHXImage);
	R2::destroy_image(engine, updateData.psiHYImage);
	R2::destroy_image(engine, updateData.psiHZImage);

	R2::destroy_descriptor(engine, updateData.initDescriptor);
	R2::destroy_descriptor(engine, updateData.eDescriptor);
	R2::destroy_descriptor(engine, updateData.hDescriptor);
}

void L3DsatUpdate::render(VkCommandBuffer cmd, L3Dsat::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex)
{
	if (updateData.initialise) {
		updateData.t = 0.0f;
		updateData.step = 0;
	}

	auto& e = updateData.eImage;
	auto& f = updateData.fImage;
	auto& j = updateData.jImage;
	auto& h = updateData.hImage;
	auto& ePeak = updateData.ePeakImage;

	auto& psiEXImage = updateData.psiEXImage;
	auto& psiEYImage = updateData.psiEYImage;
	auto& psiEZImage = updateData.psiEZImage;
	auto& psiHXImage = updateData.psiHXImage;
	auto& psiHYImage = updateData.psiHYImage;
	auto& psiHZImage = updateData.psiHZImage;

	VkDescriptorSet& initDescriptor = updateData.initDescriptor.set(frameIndex);
	VkDescriptorSet& eDescriptor = updateData.eDescriptor.set(frameIndex);
	VkDescriptorSet& hDescriptor = updateData.hDescriptor.set(frameIndex);

	updateData.descriptorAllocators[frameIndex].clear_pools(engine->_device);
	initDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.initDescriptor.layout);
	eDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.eDescriptor.layout);
	hDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.hDescriptor.layout);

	DescriptorWriter descriptorWriter;

	bool updateSimulation = updateData.step < updateData.maxStep;

	L3Dsat::UpdatePushConstants pushConstants{
		.dimensions = glm::uvec4{ updateData.x, updateData.y, updateData.z, 0 },
		.simData = glm::vec4{ updateData.dx, updateData.dt, updateData.t, updateData.c },
		.simData2 = glm::vec4{ updateData.timeUnit, updateData.lengthUnit, updateData.sourceOmega0, 0.0f },
		.simDataInt = glm::ivec4 { updateData.step, updateData.npml, updateData.maxStep, 0 },
		.ABuffer = updateData.ABuffer.address,
		.BBuffer = updateData.BBuffer.address,
		.sampleBuffer0 = updateData.sampleBuffer0.address,
		.sampleBuffer1 = updateData.sampleBuffer1.address
	};

	uint32_t dispatchX = static_cast<uint32_t>(std::ceilf(updateData.x / 8.0f));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceilf(updateData.y / 8.0f));
	uint32_t dispatchZ = static_cast<uint32_t>(std::ceilf(updateData.z / 8.0f));

	// initialise simulation dispatch
	// write: all src images

	if (updateData.initialise) {
		updateData.initialise = false;
		updateData.fileWrite = true;

		VkImage initImageTransitions[]{
			e.src().image,
			e.dst().image,
			f.src().image,
			f.dst().image,
			j.src().image,
			j.dst().image,
			h.src().image,
			h.dst().image,
			ePeak.src().image,
			ePeak.dst().image,
			psiEXImage.src().image,
			psiEXImage.dst().image,
			psiEYImage.src().image,
			psiEYImage.dst().image,
			psiEZImage.src().image,
			psiEZImage.dst().image,
			psiHXImage.src().image,
			psiHXImage.dst().image,
			psiHYImage.src().image,
			psiHYImage.dst().image,
			psiHZImage.src().image,
			psiHZImage.dst().image
		};
		for (uint32_t i = 0; i < sizeof(initImageTransitions) / sizeof(VkImage); i++) {
			vkutil::image_barrier(cmd,
				initImageTransitions[i],
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_NONE,
				VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_ASPECT_COLOR_BIT
			);
		}

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, e.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, f.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, j.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, ePeak.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		descriptorWriter.write_image(5, psiEXImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(6, psiEYImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(7, psiEZImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(8, psiHXImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(9, psiHYImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(10, psiHZImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		descriptorWriter.update_set(engine->_device, initDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.layout, 1, 1, &initDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.initPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3Dsat::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, dispatchZ);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);
	}

	if (updateSimulation) {
		// e update
		// read: ga, dz, hx, hy
		// write: dz, e

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, e.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, e.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, f.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, f.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, j.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(5, j.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(6, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(7, ePeak.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(8, ePeak.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(9, psiEXImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(10, psiEXImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(11, psiEYImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(12, psiEYImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(13, psiEZImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(14, psiEZImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, eDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.eUpdatePipeline.layout, 1, 1, &eDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.eUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.eUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3Dsat::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, dispatchZ);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		e.swap();
		f.swap();
		j.swap();
		ePeak.swap();
		psiEXImage.swap();
		psiEYImage.swap();
		psiEZImage.swap();

		// h update
		// read: e, hx, hy, ihx, ihy
		// write: hx, hy, ihx, ihy

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, e.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, h.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, psiHXImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, psiHXImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(5, psiHYImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(6, psiHYImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(7, psiHZImage.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(8, psiHZImage.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, hDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hUpdatePipeline.layout, 1, 1, &hDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.hUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3Dsat::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, dispatchZ);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		h.swap();
		psiHXImage.swap();
		psiHYImage.swap();
		psiHZImage.swap();

		// increment step
		updateData.step++;
		updateData.t = updateData.dt * updateData.step;
	}

	if (updateData.step >= updateData.maxStep && updateData.fileWrite) {
		updateData.fileWrite = false;

		std::vector<glm::vec3> sampleData0;
		std::vector<glm::vec3> sampleData1;
		sampleData0.resize(updateData.sampleBuffer0.size / sizeof(glm::vec3));
		sampleData1.resize(updateData.sampleBuffer1.size / sizeof(glm::vec3));

		memcpy(sampleData0.data(), updateData.sampleBuffer0.info.pMappedData, updateData.sampleBuffer0.size);
		memcpy(sampleData1.data(), updateData.sampleBuffer1.info.pMappedData, updateData.sampleBuffer1.size);

		const char* suffix;
		switch (updateData.updateShader) {
		case L3Dsat::GAIN:
			suffix = "_gain";
			break;
		case L3Dsat::CAVITY:
			suffix = "_cavity";
			break;
		}

		FILE* f0 = fopen(std::format("data_gpu_sim0{}.bin", suffix).c_str(), "wb");
		FILE* f1 = fopen(std::format("data_gpu_sim1{}.bin", suffix).c_str(), "wb");
		if (!f0 || !f1) {
			return;
		}

		fwrite(sampleData0.data(), sizeof(glm::vec3), updateData.maxStep, f0);
		fwrite(sampleData1.data(), sizeof(glm::vec3), updateData.maxStep, f1);

		fclose(f0);
		fclose(f1);
	}
}
