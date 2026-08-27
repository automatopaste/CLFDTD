#include "vk_tex_store.h"

#include "vk_engine.h"
#include "vk_synchronisation.h"

void TextureStore::init(VulkanEngine* engine)
{
	_engine = engine;
}

void TextureStore::cleanup() {
	for (AllocatedImage& texture : textures) {
		_engine->destroy_image(texture);
	}

	textures.clear();
}

uint32_t TextureStore::add_texture(AllocatedImage& image, VkImageLayout layout) {
	textures.push_back(image);

	imageViews.push_back(image.imageView);
	imageLayouts.push_back(layout);

	uint32_t id = loaded;
	loaded++;

	needsDescriptorRebuild = true;

	return id;
}

void TextureStore::create_empty(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t count, VkImageCreateFlags create)
{
	clear(true);

	for (uint32_t i = 0; i < count; i++) {
		AllocatedImage image = _engine->create_image(VkExtent3D{ extent.width, extent.height, 1 }, format, usage, false, create);
		add_texture(image, layout);
	}
}

void TextureStore::create_empty_attachments(VkRenderingAttachmentInfo& attachmentInfo, VkClearValue& clearValue, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t count, VkImageCreateFlags create)
{
	clear(true);

	for (uint32_t i = 0; i < count; i++) {
		AllocatedImage image = _engine->create_image(VkExtent3D{ extent.width, extent.height, 1 }, format, usage, false, create);
		add_texture(image, layout);

		VkRenderingAttachmentInfo& newInfo = imageAttachments.emplace_back();
		newInfo = attachmentInfo;
		newInfo.imageView = image.imageView;
		newInfo.clearValue = clearValue;
	}
}

void TextureStore::clear(bool deallocateImages)
{
	if (deallocateImages) {
		cleanup();
	}

	imageViews.clear();
	imageLayouts.clear();
	imageAttachments.clear();

	loaded = 0;
}

void TextureStore::transition_all(VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageAspectFlags aspect)
{
	// doesn't work properly for some reason
	//vkutil::image_barrier_count(cmd, textures.data(), textures.size(), srcStage, dstStage, srcAccess, dstAccess, oldLayout, newLayout, aspect);
}

void TextureStore::write_descriptor(VkDescriptorSet set)
{
	DescriptorWriter writer;
	writer.write_images(0, get_count(), get_views(), get_layouts(), _engine->_defaultSamplerLinear, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_engine->_device, set);
}

uint32_t TextureStore::get_count()
{
	return static_cast<uint32_t>(textures.size());
}

VkImageView* TextureStore::get_views()
{
	return imageViews.data();
}

VkImageLayout* TextureStore::get_layouts()
{
	return imageLayouts.data();
}

VkRenderingAttachmentInfo* TextureStore::get_attachments()
{
	return imageAttachments.data();
}
