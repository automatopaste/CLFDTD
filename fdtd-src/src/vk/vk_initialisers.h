#pragma once

#include "vk_types.h"

namespace vkinit {
    //> init_cmd
    VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
    VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count);
    //< init_cmd

    VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);
    VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

    VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);

    VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);

    VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
        VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkPresentInfoKHR present_info();

    VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout, VkAttachmentLoadOp load, VkAttachmentStoreOp store);

    VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);

    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

    VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

    VkSemaphoreSubmitInfo timeline_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore, uint64_t value);

    VkSemaphoreWaitInfo semaphore_wait_info(VkSemaphore* semaphore, uint64_t* waitValue);
    VkSemaphoreWaitInfo semaphore_wait_info(VkSemaphore* pSemaphores, uint64_t* waitValues, uint32_t count);

    VkSemaphoreTypeCreateInfo semaphore_type_create_info(VkSemaphoreType type, uint64_t initialValue);

    VkSemaphoreSignalInfo semaphore_signal_info(VkSemaphore semaphore, uint64_t value);

    VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags,
        uint32_t binding);
    VkDescriptorSetLayoutCreateInfo descriptorset_layout_create_info(VkDescriptorSetLayoutBinding* bindings,
        uint32_t bindingCount);
    VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
        VkDescriptorImageInfo* imageInfo, uint32_t binding);
    VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet,
        VkDescriptorBufferInfo* bufferInfo, uint32_t binding);
    VkDescriptorBufferInfo buffer_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

    VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
    VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
    VkPipelineLayoutCreateInfo pipeline_layout_create_info();
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entry = "main");
}