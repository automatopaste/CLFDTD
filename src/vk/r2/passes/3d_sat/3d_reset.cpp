#include "3d_reset.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

#include "../3d_sat_commander.h"
#include "vk/r2/r2_config.h"

#include "imgui.h"

#include "glm/glm.hpp"

void L3DsatReset::init(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

	for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {		
		R2::create_buffer(engine, &data.globalData.globalBuffers[i], sizeof(L3Dsat::GlobalData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
	}

	DescriptorLayoutBuilder builder;

	builder = DescriptorLayoutBuilder{};
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	//builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, R2::MAX_TEXTURE_COUNT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
	data.globalDescriptor.layout = builder.build(engine->_device, VK_SHADER_STAGE_ALL);
}

void L3DsatReset::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

	for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
		R2::destroy_buffer(engine, data.globalData.globalBuffers[i]);
	}

	R2::destroy_descriptor(engine, data.globalDescriptor);
}

void L3DsatReset::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex)
{
	L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

	glm::mat4 view = engine->mainCamera;

	// 3d perspective matrix (unused)

	float fov = 90.f;
	float aspect = (float)renderData.drawExtent.width / (float)renderData.drawExtent.height;
	float zNear = 0.01f;
	float f = 1.0f / tan(glm::radians(fov) / 2.0f);
	glm::mat4 projection = glm::mat4(
		f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, -f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, zNear, 0.0f
	);

	//uint32_t tilesX = static_cast<uint32_t>(std::ceil(renderData.drawExtent.width / float(R2::LIGHT_CULLING_SCREEN_TILE_SIZE)));
	//uint32_t tilesY = static_cast<uint32_t>(std::ceil(renderData.drawExtent.height / float(R2::LIGHT_CULLING_SCREEN_TILE_SIZE)));

	L3Dsat::GlobalDataBuffer globalDataBuffer{};
	globalDataBuffer.view = view;
	globalDataBuffer.proj = projection;
	globalDataBuffer.viewproj = globalDataBuffer.proj * globalDataBuffer.view;
	globalDataBuffer.invviewproj = glm::inverse(globalDataBuffer.viewproj);
	globalDataBuffer.invproj = glm::inverse(globalDataBuffer.proj);
	globalDataBuffer.invview = glm::inverse(globalDataBuffer.view);
	globalDataBuffer.ambientColor = glm::vec4(1.0f);
	globalDataBuffer.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	globalDataBuffer.sunlightDirection = glm::normalize(glm::vec4(0.2f, 1.0f, 0.2, 0.0f));
	globalDataBuffer.sunlightDirection.a = 0.5f; // specular strength
	globalDataBuffer.viewPos = engine->mainCameraPos;
	globalDataBuffer.viewport = glm::uvec4{ renderData.drawExtent.width, renderData.drawExtent.height, 0, 0 };

	memcpy(data.globalData.globalBuffers[frameIndex].info.pMappedData, &globalDataBuffer, sizeof(L3Dsat::GlobalData));

	VkDescriptorSet& globalDescriptor = data.globalDescriptor.set(frameIndex);
	globalDescriptor = renderData.descriptorAllocators[frameIndex].allocate(engine->_device, data.globalDescriptor.layout, 1, 255); // allocate non-zero number of textures (placeholder)

	DescriptorWriter descriptorWriter{};

	descriptorWriter.write_buffer(0, data.globalData.globalBuffers[frameIndex].buffer, sizeof(L3Dsat::GlobalData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptorWriter.write_image(1, renderData.drawImage.image.imageView, engine->_defaultSamplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorWriter.update_set(engine->_device, globalDescriptor);
}
