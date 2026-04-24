#include "r2_update.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"
#include "vk/vk_images.h"

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
	data.npml = 8;
	data.x = 1600;
	data.y = 900;
	data.z = 1;
	data.initialise = true;
	data.fileWrite = true;

	size_t sizeX = data.x * sizeof(float);
	size_t sizeY = data.y * sizeof(float);

	R2::create_buffer(engine, &data.pmlBuffers, sizeof(R2::PMLBuffers), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);

	R2::create_buffer(engine, &data.gi2Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.gi3Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &data.gj2Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.gj3Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &data.fi1Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.fi2Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.fi3Buffer, sizeX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	R2::create_buffer(engine, &data.fj1Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.fj2Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
	R2::create_buffer(engine, &data.fj3Buffer, sizeY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

	constexpr uint32_t sampleStepCount = 12000;
	constexpr size_t sampleBufferSize = sampleStepCount * sizeof(float);
	R2::create_buffer(engine, &data.sampleBuffer, sampleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);

	R2::PMLBuffers pmlBuffers{
		.gi2Buffer = data.gi2Buffer.address,
		.gi3Buffer = data.gi3Buffer.address,
		.gj2Buffer = data.gj2Buffer.address,
		.gj3Buffer = data.gj3Buffer.address,
		.fi1Buffer = data.fi1Buffer.address,
		.fi2Buffer = data.fi2Buffer.address,
		.fi3Buffer = data.fi3Buffer.address,
		.fj1Buffer = data.fj1Buffer.address,
		.fj2Buffer = data.fj2Buffer.address,
		.fj3Buffer = data.fj3Buffer.address
	};
	memcpy(data.pmlBuffers.info.pMappedData, &pmlBuffers, sizeof(R2::PMLBuffers));

	VkExtent3D size = { data.x, data.y, data.z };
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	R2::create_image(engine, &data.ezImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.dzImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.hxImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.hyImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.gaImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.ihxImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);
	R2::create_image(engine, &data.ihyImage, size, VK_FORMAT_R32_SFLOAT, usage, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ezSrc
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxSrc
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hySrc
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihxSrc
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ihySrc
	renderData.initDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ezDst
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzSrc
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // dzDst
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // gaSrc
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hxSrc
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // hySrc
	renderData.ezDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

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
	renderData.hxyDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(R2::UpdatePushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayout initLayouts[] = { renderData.globalDescriptor.layout, renderData.initDescriptor.layout };
	uint32_t initLayoutCount = sizeof(initLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo initLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	initLayoutInfo.pPushConstantRanges = &pushConstantRange;
	initLayoutInfo.pushConstantRangeCount = 1;
	initLayoutInfo.pSetLayouts = initLayouts;
	initLayoutInfo.setLayoutCount = initLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &initLayoutInfo, nullptr, &renderData.updateData.initPipeline.layout));

	VkDescriptorSetLayout ezLayouts[] = { renderData.globalDescriptor.layout, renderData.ezDescriptor.layout };
	uint32_t ezLayoutCount = sizeof(ezLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo ezLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	ezLayoutInfo.pPushConstantRanges = &pushConstantRange;
	ezLayoutInfo.pushConstantRangeCount = 1;
	ezLayoutInfo.pSetLayouts = ezLayouts;
	ezLayoutInfo.setLayoutCount = ezLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &ezLayoutInfo, nullptr, &renderData.updateData.ezUpdatePipeline.layout));

	VkDescriptorSetLayout hxyLayouts[] = { renderData.globalDescriptor.layout, renderData.hxyDescriptor.layout };
	uint32_t hxyLayoutCount = sizeof(hxyLayouts) / sizeof(VkDescriptorSetLayout);
	VkPipelineLayoutCreateInfo hxyLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	hxyLayoutInfo.pPushConstantRanges = &pushConstantRange;
	hxyLayoutInfo.pushConstantRangeCount = 1;
	hxyLayoutInfo.pSetLayouts = hxyLayouts;
	hxyLayoutInfo.setLayoutCount = hxyLayoutCount;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &hxyLayoutInfo, nullptr, &renderData.updateData.hxyUpdatePipeline.layout));

	VkShaderModule initShader;
	VkShaderModule ezShader;
	VkShaderModule hxyShader;
	vkutil::check_compile_load("r2_initialise.comp", engine, &initShader, engine->_compileShaders);
	vkutil::check_compile_load("r2_ez_update.comp", engine, &ezShader, engine->_compileShaders);
	vkutil::check_compile_load("r2_hxy_update.comp", engine, &hxyShader, engine->_compileShaders);

	ComputePipelineBuilder computePipelineBuilder;

	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(initShader);
	computePipelineBuilder.set_pipeline_layout(renderData.updateData.initPipeline.layout);
	computePipelineBuilder.build(engine->_device, renderData.updateData.initPipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(ezShader);
	computePipelineBuilder.set_pipeline_layout(renderData.updateData.ezUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, renderData.updateData.ezUpdatePipeline.pipeline);
	computePipelineBuilder = ComputePipelineBuilder{};
	computePipelineBuilder.set_shader(hxyShader);
	computePipelineBuilder.set_pipeline_layout(renderData.updateData.hxyUpdatePipeline.layout);
	computePipelineBuilder.build(engine->_device, renderData.updateData.hxyUpdatePipeline.pipeline);

	vkDestroyShaderModule(engine->_device, initShader, nullptr);
	vkDestroyShaderModule(engine->_device, ezShader, nullptr);
	vkDestroyShaderModule(engine->_device, hxyShader, nullptr);
}

void Update::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	Update::Data& data = renderData.updateData;

	R2::destroy_buffer(engine, data.pmlBuffers);
	R2::destroy_buffer(engine, data.gi2Buffer);
	R2::destroy_buffer(engine, data.gi3Buffer);
	R2::destroy_buffer(engine, data.gj2Buffer);
	R2::destroy_buffer(engine, data.gj3Buffer);
	R2::destroy_buffer(engine, data.fi1Buffer);
	R2::destroy_buffer(engine, data.fi2Buffer);
	R2::destroy_buffer(engine, data.fi3Buffer);
	R2::destroy_buffer(engine, data.fj1Buffer);
	R2::destroy_buffer(engine, data.fj2Buffer);
	R2::destroy_buffer(engine, data.fj3Buffer);

	R2::destroy_buffer(engine, data.sampleBuffer);

	R2::destroy_image(engine, data.ezImage);
	R2::destroy_image(engine, data.dzImage);
	R2::destroy_image(engine, data.hxImage);
	R2::destroy_image(engine, data.hyImage);
	R2::destroy_image(engine, data.gaImage);
	R2::destroy_image(engine, data.ihxImage);
	R2::destroy_image(engine, data.ihyImage);

	R2::destroy_descriptor(engine, renderData.initDescriptor);
	R2::destroy_descriptor(engine, renderData.ezDescriptor);
	R2::destroy_descriptor(engine, renderData.hxyDescriptor);
}

void Update::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex)
{
	Update::Data& data = renderData.updateData;

	if (ImGui::Begin("Simulation Options")) {
		data.initialise |= ImGui::Button("Restart");
	}
	ImGui::End();

	if (data.initialise) {
		data.t = 0.0f;
		data.step = 0;
	}

	auto& ez = data.ezImage;
	auto& dz = data.dzImage;
	auto& hx = data.hxImage;
	auto& hy = data.hyImage;
	auto& ga = data.gaImage;
	auto& ihx = data.ihxImage;
	auto& ihy = data.ihyImage;

	VkDescriptorSet& initDescriptor = renderData.initDescriptor.set(frameIndex);
	VkDescriptorSet& ezDescriptor = renderData.ezDescriptor.set(frameIndex);
	VkDescriptorSet& hxyDescriptor = renderData.hxyDescriptor.set(frameIndex);

	initDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.initDescriptor.layout);
	ezDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.ezDescriptor.layout);
	hxyDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.hxyDescriptor.layout);

	DescriptorWriter descriptorWriter;

	const int maxSimStep = 12000;
	bool updateSimulation = data.step < maxSimStep;

	if (data.step >= maxSimStep && data.fileWrite) {
		data.fileWrite = false;

		std::vector<float> sampleData;
		sampleData.resize(data.sampleBuffer.size / sizeof(float));
		memcpy(sampleData.data(), data.sampleBuffer.info.pMappedData, data.sampleBuffer.size);

		FILE* f = fopen("data_gpu_sim.bin", "wb");
		if (!f) {
			return;
		}

		fwrite(sampleData.data(), sizeof(float), maxSimStep, f);
		fclose(f);
	}

	R2::UpdatePushConstants pushConstants{
		.dimensions = glm::uvec4{ data.x, data.y, data.z, 0 },
		.simData = glm::vec4{ data.ddx, data.dt, data.t, 0.0f },
		.simDataInt = glm::ivec4 { data.step, data.npml, maxSimStep, 0 },
		.pmlBuffers = data.pmlBuffers.address,
		.sampleBuffer = data.sampleBuffer.address
	};

	uint32_t dispatchX = static_cast<uint32_t>(std::ceilf(data.x / 8.0f));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceilf(data.y / 8.0f));

	// initialise simulation dispatch
	// write: all src images

	if (data.initialise) {
		data.initialise = false;
		data.fileWrite = true;

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

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, data.initPipeline.layout, 1, 1, &initDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderData.updateData.initPipeline.pipeline);
		vkCmdPushConstants(cmd, renderData.updateData.initPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(R2::UpdatePushConstants), &pushConstants);
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

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, data.ezUpdatePipeline.layout, 1, 1, &ezDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderData.updateData.ezUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, renderData.updateData.ezUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(R2::UpdatePushConstants), &pushConstants);
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

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, data.hxyUpdatePipeline.layout, 1, 1, &hxyDescriptor, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderData.updateData.hxyUpdatePipeline.pipeline);
		vkCmdPushConstants(cmd, renderData.updateData.hxyUpdatePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(R2::UpdatePushConstants), &pushConstants);
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
		data.step++;
		data.t += data.dt;
	}
}
