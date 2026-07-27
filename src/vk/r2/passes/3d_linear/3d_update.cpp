#include "3d_update.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"
#include "vk/vk_images.h"

#include "vk/r2/r2_config.h"

#include "imgui.h"

#include "glm/glm.hpp"

void L3DUpdate::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3D::Data& data = renderData.get_user_data<L3D::Data>();

	L3D::UpdateData& updateData = data.updateData;

	updateData.ddx = 0.001f;
	updateData.dt = updateData.ddx / 6e8f;
	updateData.t = 0.0f;
	updateData.step = 0;
	updateData.npml = 8;
	updateData.x = 300;
	updateData.y = 300;
	updateData.z = 50;
	updateData.initialise = true;
	updateData.fileWrite = true;
	updateData.maxStep = 200;

	size_t sizeX = updateData.x * sizeof(float);
	size_t sizeY = updateData.y * sizeof(float);
	size_t sizeZ = updateData.z * sizeof(float);

	/*
	R2::create_buffer(engine, &updateData.pmlBuffers, sizeof(L3D::PMLBuffers), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);

	R2::create_buffer(engine, &updateData.gi2Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.gi3Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &updateData.gj2Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.gj3Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &updateData.fi1Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.fi2Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.fi3Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &updateData.fj1Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.fj2Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &updateData.fj3Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	*/

	size_t sampleBufferSize = updateData.maxStep * sizeof(glm::vec3);
	R2::create_buffer(engine, &updateData.sampleBuffer, sampleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);

	/*
	D2Disp::PMLBuffers pmlBuffers{
		.gi2Buffer = updateData.gi2Buffer.address,
		.gi3Buffer = updateData.gi3Buffer.address,
		.gj2Buffer = updateData.gj2Buffer.address,
		.gj3Buffer = updateData.gj3Buffer.address,
		.fi1Buffer = updateData.fi1Buffer.address,
		.fi2Buffer = updateData.fi2Buffer.address,
		.fi3Buffer = updateData.fi3Buffer.address,
		.fj1Buffer = updateData.fj1Buffer.address,
		.fj2Buffer = updateData.fj2Buffer.address,
		.fj3Buffer = updateData.fj3Buffer.address
	};
	memcpy(updateData.pmlBuffers.info.pMappedData, &pmlBuffers, sizeof(D2Disp::PMLBuffers));
	*/

	VkExtent3D size = { updateData.x, updateData.y, updateData.z };
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	R2::create_image(engine, &updateData.gaImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.dImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.eImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.hImage, size, VK_FORMAT_R32G32B32A32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkImage initImageTransitions[]{
			updateData.gaImage.src().image,
			updateData.gaImage.dst().image,
			updateData.dImage.src().image,
			updateData.dImage.dst().image,
			updateData.eImage.src().image,
			updateData.eImage.dst().image,
			updateData.hImage.src().image,
			updateData.hImage.dst().image
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
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dDst
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // eDst
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hDst
	updateData.initDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dDst
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // eDst
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hSrc
	updateData.eDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // eSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hDst
	updateData.hDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(L3D::UpdatePushConstants);
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

	VkShaderModule initShader;
	VkShaderModule eShader;
	VkShaderModule hShader;
	vkutil::check_compile_load("3d_linear/3d_initialise.comp", engine, &initShader, engine->_compileShaders);
	vkutil::check_compile_load("3d_linear/3d_e_update.comp", engine, &eShader, engine->_compileShaders);
	vkutil::check_compile_load("3d_linear/3d_h_update.comp", engine, &hShader, engine->_compileShaders);

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

void L3DUpdate::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3D::Data& data = renderData.get_user_data<L3D::Data>();
	L3D::UpdateData& updateData = data.updateData;

	/*R2::destroy_buffer(engine, updateData.pmlBuffers);
	R2::destroy_buffer(engine, updateData.gi2Buffer);
	R2::destroy_buffer(engine, updateData.gi3Buffer);
	R2::destroy_buffer(engine, updateData.gj2Buffer);
	R2::destroy_buffer(engine, updateData.gj3Buffer);
	R2::destroy_buffer(engine, updateData.fi1Buffer);
	R2::destroy_buffer(engine, updateData.fi2Buffer);
	R2::destroy_buffer(engine, updateData.fi3Buffer);
	R2::destroy_buffer(engine, updateData.fj1Buffer);
	R2::destroy_buffer(engine, updateData.fj2Buffer);
	R2::destroy_buffer(engine, updateData.fj3Buffer);*/

	R2::destroy_buffer(engine, updateData.sampleBuffer);

	R2::destroy_image(engine, updateData.gaImage);
	R2::destroy_image(engine, updateData.dImage);
	R2::destroy_image(engine, updateData.eImage);
	R2::destroy_image(engine, updateData.hImage);

	R2::destroy_descriptor(engine, updateData.initDescriptor);
	R2::destroy_descriptor(engine, updateData.eDescriptor);
	R2::destroy_descriptor(engine, updateData.hDescriptor);
}

void L3DUpdate::render(VkCommandBuffer cmd, L3D::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex)
{
	if (updateData.initialise) {
		updateData.t = 0.0f;
		updateData.step = 0;
	}

	auto& ga = updateData.gaImage;
	auto& d = updateData.dImage;
	auto& e = updateData.eImage;
	auto& h = updateData.hImage;

	VkDescriptorSet& initDescriptor = updateData.initDescriptor.set(frameIndex);
	VkDescriptorSet& eDescriptor = updateData.eDescriptor.set(frameIndex);
	VkDescriptorSet& hDescriptor = updateData.hDescriptor.set(frameIndex);

	initDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.initDescriptor.layout);
	eDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.eDescriptor.layout);
	hDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.hDescriptor.layout);

	DescriptorWriter descriptorWriter;

	bool updateSimulation = updateData.step < updateData.maxStep;

	if (updateData.step >= updateData.maxStep && updateData.fileWrite) {
		updateData.fileWrite = false;

		std::vector<glm::vec3> sampleData;
		sampleData.resize(updateData.sampleBuffer.size / sizeof(glm::vec3));
		memcpy(sampleData.data(), updateData.sampleBuffer.info.pMappedData, updateData.sampleBuffer.size);

		FILE* f = fopen("data_gpu_sim.bin", "wb");
		if (!f) {
			return;
		}

		fwrite(sampleData.data(), sizeof(glm::vec3), updateData.maxStep, f);
		fclose(f);
	}

	L3D::UpdatePushConstants pushConstants{
		.dimensions = glm::uvec4{ updateData.x, updateData.y, updateData.z, 0 },
		.simData = glm::vec4{ updateData.ddx, updateData.dt, updateData.t, 0.0f },
		.simDataInt = glm::ivec4 { updateData.step, updateData.npml, updateData.maxStep, 0 },
		//.pmlBuffers = updateData.pmlBuffers.address,
		.sampleBuffer = updateData.sampleBuffer.address
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
			ga.src().image,
			ga.dst().image,
			d.src().image,
			d.dst().image,
			e.src().image,
			e.dst().image,
			h.src().image,
			h.dst().image
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
		descriptorWriter.write_image(0, ga.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, d.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, e.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, initDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.layout, 1, 1, &initDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.initPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3D::UpdatePushConstants), &pushConstants);
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
		descriptorWriter.write_image(0, ga.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, d.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, d.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, e.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, eDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.eUpdatePipeline.layout, 1, 1, &eDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.eUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.eUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3D::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, dispatchZ);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		d.swap();
		e.swap();

		// h update
		// read: e, hx, hy, ihx, ihy
		// write: hx, hy, ihx, ihy

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, e.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, h.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, h.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, hDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hUpdatePipeline.layout, 1, 1, &hDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.hUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(L3D::UpdatePushConstants), &pushConstants);
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

		// increment step
		updateData.step++;
		updateData.t += updateData.dt;
	}
}
