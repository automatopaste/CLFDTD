#include "2d_update.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"
#include "vk/vk_images.h"

#include "../2d_basic_commander.h"
#include "vk/r2/r2_config.h"

#include "imgui.h"

#include "glm/glm.hpp"

void Update::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	D2::Data& data = renderData.get_user_data<D2::Data>();

	D2::UpdateData& updateData = data.updateData;

	updateData.ddx = 0.01f;
	updateData.dt = updateData.ddx / 6e8f;
	updateData.t = 0.0f;
	updateData.step = 0;
	updateData.npml = 8;
	updateData.x = 800;
	updateData.y = 800;
	updateData.z = 1;
	updateData.initialise = true;
	updateData.fileWrite = true;

	size_t sizeX = updateData.x * sizeof(float);
	size_t sizeY = updateData.y * sizeof(float);

	R2::create_buffer(engine, &updateData.pmlBuffers, sizeof(D2::PMLBuffers), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);

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

	constexpr uint32_t sampleStepCount = 12000;
	constexpr size_t sampleBufferSize = sampleStepCount * sizeof(float);
	R2::create_buffer(engine, &updateData.sampleBuffer, sampleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);

	D2::PMLBuffers pmlBuffers{
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
	memcpy(updateData.pmlBuffers.info.pMappedData, &pmlBuffers, sizeof(D2::PMLBuffers));

	VkExtent3D size = { updateData.x, updateData.y, updateData.z };
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	R2::create_image(engine, &updateData.ezImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.dzImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.hxImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.hyImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.gaImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.ihxImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &updateData.ihyImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ezSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxSrc
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hySrc
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihxSrc
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihySrc
	updateData.initDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ezDst
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzDst
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxSrc
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hySrc
	updateData.ezDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ezSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxDst
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hySrc
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hyDst
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihxSrc
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihxDst
	builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihySrc
	builder.add_binding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihyDst
	updateData.hxyDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(D2::UpdatePushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayout initLayouts[] = { data.globalDescriptor.layout, updateData.initDescriptor.layout };
	uint32_t initLayoutCount = sizeof(initLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo initLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	initLayoutInfo.pPushConstantRanges = &pushConstantRange;
	initLayoutInfo.pushConstantRangeCount = 1;
	initLayoutInfo.pSetLayouts = initLayouts;
	initLayoutInfo.setLayoutCount = initLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &initLayoutInfo, nullptr, &data.updateData.initPipeline.layout));

	VkDescriptorSetLayout ezLayouts[] = { data.globalDescriptor.layout, updateData.ezDescriptor.layout };
	uint32_t ezLayoutCount = sizeof(ezLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo ezLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	ezLayoutInfo.pPushConstantRanges = &pushConstantRange;
	ezLayoutInfo.pushConstantRangeCount = 1;
	ezLayoutInfo.pSetLayouts = ezLayouts;
	ezLayoutInfo.setLayoutCount = ezLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &ezLayoutInfo, nullptr, &data.updateData.ezUpdatePipeline.layout));

	VkDescriptorSetLayout hxyLayouts[] = { data.globalDescriptor.layout, updateData.hxyDescriptor.layout };
	uint32_t hxyLayoutCount = sizeof(hxyLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo hxyLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	hxyLayoutInfo.pPushConstantRanges = &pushConstantRange;
	hxyLayoutInfo.pushConstantRangeCount = 1;
	hxyLayoutInfo.pSetLayouts = hxyLayouts;
	hxyLayoutInfo.setLayoutCount = hxyLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &hxyLayoutInfo, nullptr, &data.updateData.hxyUpdatePipeline.layout));

	VkShaderModule initShader;
	VkShaderModule ezShader;
	VkShaderModule hxyShader;
	vkutil::check_compile_load("2d_basic/2d_initialise.comp", engine, &initShader, engine->_compileShaders);
	vkutil::check_compile_load("2d_basic/2d_ez_update.comp", engine, &ezShader, engine->_compileShaders);
	vkutil::check_compile_load("2d_basic/2d_hxy_update.comp", engine, &hxyShader, engine->_compileShaders);

	ComputePipelineBuilder computePipelineBuilder;

	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(initShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.initPipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.initPipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(ezShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.ezUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.ezUpdatePipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(hxyShader);
	computePipelineBuilder.set_pipeline_layout(data.updateData.hxyUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, data.updateData.hxyUpdatePipeline.pipeline);

	vkDestroyShaderModule(engine->_device, initShader, nullptr);
	vkDestroyShaderModule(engine->_device, ezShader, nullptr);
	vkDestroyShaderModule(engine->_device, hxyShader, nullptr);
}

void Update::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	D2::Data& data = renderData.get_user_data<D2::Data>();
	D2::UpdateData& updateData = data.updateData;

	R2::destroy_buffer(engine, updateData.pmlBuffers);
	R2::destroy_buffer(engine, updateData.gi2Buffer);
	R2::destroy_buffer(engine, updateData.gi3Buffer);
	R2::destroy_buffer(engine, updateData.gj2Buffer);
	R2::destroy_buffer(engine, updateData.gj3Buffer);
	R2::destroy_buffer(engine, updateData.fi1Buffer);
	R2::destroy_buffer(engine, updateData.fi2Buffer);
	R2::destroy_buffer(engine, updateData.fi3Buffer);
	R2::destroy_buffer(engine, updateData.fj1Buffer);
	R2::destroy_buffer(engine, updateData.fj2Buffer);
	R2::destroy_buffer(engine, updateData.fj3Buffer);

	R2::destroy_buffer(engine, updateData.sampleBuffer);

	R2::destroy_image(engine, updateData.ezImage);
	R2::destroy_image(engine, updateData.dzImage);
	R2::destroy_image(engine, updateData.hxImage);
	R2::destroy_image(engine, updateData.hyImage);
	R2::destroy_image(engine, updateData.gaImage);
	R2::destroy_image(engine, updateData.ihxImage);
	R2::destroy_image(engine, updateData.ihyImage);

	R2::destroy_descriptor(engine, updateData.initDescriptor);
	R2::destroy_descriptor(engine, updateData.ezDescriptor);
	R2::destroy_descriptor(engine, updateData.hxyDescriptor);
}

void Update::render(VkCommandBuffer cmd, D2::UpdateData& updateData, VulkanEngine* engine, uint32_t frameIndex)
{
	if (updateData.initialise) {
		updateData.t = 0.0f;
		updateData.step = 0;
	}

	auto& ez = updateData.ezImage;
	auto& dz = updateData.dzImage;
	auto& hx = updateData.hxImage;
	auto& hy = updateData.hyImage;
	auto& ga = updateData.gaImage;
	auto& ihx = updateData.ihxImage;
	auto& ihy = updateData.ihyImage;

	VkDescriptorSet& initDescriptor = updateData.initDescriptor.set(frameIndex);
	VkDescriptorSet& ezDescriptor = updateData.ezDescriptor.set(frameIndex);
	VkDescriptorSet& hxyDescriptor = updateData.hxyDescriptor.set(frameIndex);

	initDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.initDescriptor.layout);
	ezDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.ezDescriptor.layout);
	hxyDescriptor = updateData.descriptorAllocators[frameIndex].allocate(engine->_device, updateData.hxyDescriptor.layout);

	DescriptorWriter descriptorWriter;

	const int maxSimStep = 12000;
	bool updateSimulation = updateData.step < maxSimStep;

	if (updateData.step >= maxSimStep && updateData.fileWrite) {
		updateData.fileWrite = false;

		std::vector<float> sampleData;
		sampleData.resize(updateData.sampleBuffer.size / sizeof(float));
		memcpy(sampleData.data(), updateData.sampleBuffer.info.pMappedData, updateData.sampleBuffer.size);

		FILE* f = fopen("data_gpu_sim.bin", "wb");
		if (!f) {
			return;
		}

		fwrite(sampleData.data(), sizeof(float), maxSimStep, f);
		fclose(f);
	}

	D2::UpdatePushConstants pushConstants{
		.dimensions = glm::uvec4{ updateData.x, updateData.y, updateData.z, 0 },
		.simData = glm::vec4{ updateData.ddx, updateData.dt, updateData.t, 0.0f },
		.simDataInt = glm::ivec4 { updateData.step, updateData.npml, maxSimStep, 0 },
		.pmlBuffers = updateData.pmlBuffers.address,
		.sampleBuffer = updateData.sampleBuffer.address
	};

	uint32_t dispatchX = static_cast<uint32_t>(std::ceilf(updateData.x / 8.0f));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceilf(updateData.y / 8.0f));

	// initialise simulation dispatch
	// write: all src images

	if (updateData.initialise) {
		updateData.initialise = false;
		updateData.fileWrite = true;

		VkImage initImageTransitions[]{
			ez.src().image,
			ez.dst().image,
			dz.src().image,
			dz.dst().image,
			hx.src().image,
			hx.dst().image,
			hy.src().image,
			hy.dst().image,
			ga.src().image,
			ga.dst().image,
			ihx.src().image,
			ihx.dst().image,
			ihy.src().image,
			ihy.dst().image
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
		descriptorWriter.write_image(0, ez.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, dz.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, hx.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, hy.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, ga.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(5, ihx.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(6, ihy.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, initDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.layout, 1, 1, &initDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.initPipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.initPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(D2::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		ez.swap();
		dz.swap();
		hx.swap();
		hy.swap();
		ga.swap();
		ihx.swap();
		ihy.swap();
	}

	if (updateSimulation) {
		// ez update
		// read: ga, dz, hx, hy
		// write: dz, ez

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, ez.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, dz.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, dz.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, ga.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, hx.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(5, hy.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, ezDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.ezUpdatePipeline.layout, 1, 1, &ezDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.ezUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.ezUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(D2::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		dz.swap();
		ez.swap();

		// hxy update
		// read: ez, hx, hy, ihx, ihy
		// write: hx, hy, ihx, ihy

		descriptorWriter = DescriptorWriter{};
		descriptorWriter.write_image(0, ez.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(1, hx.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(2, hx.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(3, hy.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(4, hy.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(5, ihx.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(6, ihx.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(7, ihy.src().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.write_image(8, ihy.dst().imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		descriptorWriter.update_set(engine->_device, hxyDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hxyUpdatePipeline.layout, 1, 1, &hxyDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updateData.hxyUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, updateData.hxyUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(D2::UpdatePushConstants), &pushConstants);
		vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

		vkutil::memory_barrier(
			cmd,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		// swap buffers written to
		hx.swap();
		hy.swap();
		ihx.swap();
		ihy.swap();

		// increment step
		updateData.step++;
		updateData.t += updateData.dt;
	}
}
