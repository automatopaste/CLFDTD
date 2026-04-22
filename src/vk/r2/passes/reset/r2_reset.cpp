#include "r2_reset.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

#include "vk/r2/r2_config.h"
#include "vk/r2/r2_shader_structs.h"

#include "imgui.h"

#include "glm/glm.hpp"

void Reset::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
		R2::FrameData& frameData = renderData.frameData[i];
		
		R2::create_buffer(engine, &frameData.globalBuffer, sizeof(R2::GlobalData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
	}

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	//builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, R2::MAX_TEXTURE_COUNT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
	renderData.globalDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_ALL);
}

void Reset::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
		R2::FrameData& frameData = renderData.frameData[i];

		R2::destroy_buffer(engine, frameData.globalBuffer);
	}

	R2::destroy_descriptor(engine, renderData.globalDescriptor);
}

void Reset::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex)
{
	R2::FrameData& frameData = renderData.frameData[frameIndex];

	glm::mat4 view = engine->mainCamera;

	// 3d perspective matrix (unused)

	float fov = 90.f;
	float aspect = (float)renderData.drawExtent.width / (float)renderData.drawExtent.height;
	float zNear = 0.01f;
	float f = 1.0f / tan(glm::radians(fov) / 2.0f);

	glm::mat4 projection = glm::mat4( // infinite draw distance
		f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, -f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, zNear, 0.0f
	);

	//uint32_t tilesX = static_cast<uint32_t>(std::ceil(renderData.drawExtent.width / float(R2::LIGHT_CULLING_SCREEN_TILE_SIZE)));
	//uint32_t tilesY = static_cast<uint32_t>(std::ceil(renderData.drawExtent.height / float(R2::LIGHT_CULLING_SCREEN_TILE_SIZE)));

	R2::GlobalData globalData{};
	globalData.view = view;
	globalData.proj = projection;
	globalData.viewproj = globalData.proj * globalData.view;
	globalData.invviewproj = glm::inverse(globalData.viewproj);
	globalData.invproj = glm::inverse(globalData.proj);
	globalData.invview = glm::inverse(globalData.view);
	globalData.ambientColor = glm::vec4(1.0f);
	globalData.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	globalData.sunlightDirection = glm::normalize(glm::vec4(0.2f, 1.0f, 0.2, 0.0f));
	globalData.sunlightDirection.a = 0.5f; // specular strength
	globalData.viewPos = engine->mainCameraPos;
	globalData.viewport = glm::uvec4{ renderData.drawExtent.width, renderData.drawExtent.height, 0, 0 };

	memcpy(frameData.globalBuffer.info.pMappedData, &globalData, sizeof(R2::GlobalData));

	VkDescriptorSet& globalDescriptor = renderData.globalDescriptor.set(frameIndex);
	globalDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, renderData.globalDescriptor.layout, 1, 255); // allocate non-zero number of textures (placeholder)

	DescriptorWriter descriptorWriter{};

	descriptorWriter.write_buffer(0, frameData.globalBuffer.buffer, sizeof(R2::GlobalData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptorWriter.write_image(1, renderData.drawImage.image.imageView, engine->_defaultSamplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	//descriptorWriter.write_images(1, drawCache.materialTextures.get_count(), drawCache.materialTextures.get_views(), drawCache.materialTextures.get_layouts(), engine->_defaultSamplerNearest, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorWriter.update_set(engine->_device, globalDescriptor);

	/*vkutil::image_barrier(
		cmd,
		renderData.frameData[frameIndex].swapchainImage,
		VK_PIPELINE_STAGE_2_NONE,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_NONE,
		VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	);*/
}
