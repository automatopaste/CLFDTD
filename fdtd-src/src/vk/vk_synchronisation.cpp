#include "vk_synchronisation.h"

#include <iostream>

#include "vk_images.h"

namespace vkutil {

	void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect) {
		transition_image(cmd, image, oldLayout, newLayout, aspect, 0, 1);
	}

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount)
	{
		VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

		imageBarrier.oldLayout = oldLayout;
		imageBarrier.newLayout = newLayout;

		VkImageAspectFlags aspectMask = aspect;
		imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
		imageBarrier.subresourceRange.baseMipLevel = baseMipLevel;
		imageBarrier.subresourceRange.levelCount = mipLevelCount;

		imageBarrier.image = image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void image_barrier(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect)
	{
		image_barrier(cmd, image, srcStage, dstStage, srcAccess, dstAccess, oldLayout, newLayout, aspect, 0, 1);
	}

	void image_barrier(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount)
	{
		VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = srcStage;
		imageBarrier.srcAccessMask = srcAccess;
		imageBarrier.dstStageMask = dstStage;
		imageBarrier.dstAccessMask = dstAccess;

		imageBarrier.oldLayout = oldLayout;
		imageBarrier.newLayout = newLayout;

		imageBarrier.subresourceRange = vkinit::image_subresource_range(aspect);
		imageBarrier.subresourceRange.baseMipLevel = baseMipLevel;
		imageBarrier.subresourceRange.levelCount = mipLevelCount;

		imageBarrier.image = image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void image_barrier_mipchain_generate(VkCommandBuffer cmd, VkImage image, VkFilter filter, VkExtent2D imageSize, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect, uint32_t baseMipLevel, uint32_t mipLevelCount)
	{
		image_barrier(cmd, image, srcStage, VK_PIPELINE_STAGE_2_BLIT_BIT, srcAccess, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspect, baseMipLevel, mipLevelCount);

		for (uint32_t mip = baseMipLevel; mip < mipLevelCount - baseMipLevel; mip++) {
			VkExtent2D halfSize = imageSize;
			halfSize.width /= 2;
			halfSize.height /= 2;

			VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr };

			imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
			imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;

			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			imageBarrier.subresourceRange = vkinit::image_subresource_range(aspect);
			imageBarrier.subresourceRange.levelCount = 1;
			imageBarrier.subresourceRange.baseMipLevel = mip;
			imageBarrier.image = image;

			VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
			depInfo.imageMemoryBarrierCount = 1;
			depInfo.pImageMemoryBarriers = &imageBarrier;

			vkCmdPipelineBarrier2(cmd, &depInfo);

			if (mip < mipLevelCount - baseMipLevel - 1) {
				VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

				blitRegion.srcOffsets[1].x = imageSize.width;
				blitRegion.srcOffsets[1].y = imageSize.height;
				blitRegion.srcOffsets[1].z = 1;

				blitRegion.dstOffsets[1].x = halfSize.width;
				blitRegion.dstOffsets[1].y = halfSize.height;
				blitRegion.dstOffsets[1].z = 1;

				blitRegion.srcSubresource.aspectMask = aspect;
				blitRegion.srcSubresource.baseArrayLayer = 0;
				blitRegion.srcSubresource.layerCount = 1;
				blitRegion.srcSubresource.mipLevel = mip;

				blitRegion.dstSubresource.aspectMask = aspect;
				blitRegion.dstSubresource.baseArrayLayer = 0;
				blitRegion.dstSubresource.layerCount = 1;
				blitRegion.dstSubresource.mipLevel = mip + 1;

				VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
				blitInfo.dstImage = image;
				blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				blitInfo.srcImage = image;
				blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				blitInfo.filter = filter;
				blitInfo.regionCount = 1;
				blitInfo.pRegions = &blitRegion;

				vkCmdBlitImage2(cmd, &blitInfo);

				imageSize = halfSize;
			}
		}

		// transition all mip levels into the final read_only layout
		image_barrier(cmd, image, VK_PIPELINE_STAGE_2_BLIT_BIT, dstStage, VK_ACCESS_2_TRANSFER_WRITE_BIT, dstAccess, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newLayout, aspect, baseMipLevel, mipLevelCount);
	}

	void image_barrier_count(VkCommandBuffer cmd, VkImage* images, uint32_t count, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect)
	{
		std::vector<VkImageMemoryBarrier2> barriers;
		barriers.reserve(count);

		for (uint32_t i = 0; i < count; i++) {
			VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
			imageBarrier.pNext = nullptr;

			imageBarrier.srcStageMask = srcStage;
			imageBarrier.srcAccessMask = srcAccess;
			imageBarrier.dstStageMask = dstStage;
			imageBarrier.dstAccessMask = dstAccess;

			imageBarrier.oldLayout = oldLayout;
			imageBarrier.newLayout = newLayout;

			imageBarrier.subresourceRange = vkinit::image_subresource_range(aspect);
			imageBarrier.image = images[i];

			barriers.push_back(imageBarrier);
		}

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = count;
		depInfo.pImageMemoryBarriers = barriers.data();

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void memory_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
	{
		VkMemoryBarrier2 memoryBarrier{ .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		memoryBarrier.pNext = nullptr;

		memoryBarrier.srcStageMask = srcStage;
		memoryBarrier.srcAccessMask = srcAccess;
		memoryBarrier.dstStageMask = dstStage;
		memoryBarrier.dstAccessMask = dstAccess;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.memoryBarrierCount = 1;
		depInfo.pMemoryBarriers = &memoryBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void buffer_barrier(VkCommandBuffer cmd, VkBuffer buffer, uint32_t queue, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
	{
		VkBufferMemoryBarrier2 bufferBarrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr };
		bufferBarrier.buffer = buffer;
		
		bufferBarrier.srcStageMask = srcStage;
		bufferBarrier.srcAccessMask = srcAccess;
		bufferBarrier.dstStageMask = dstStage;
		bufferBarrier.dstAccessMask = dstAccess;

		bufferBarrier.srcQueueFamilyIndex = queue;
		bufferBarrier.dstQueueFamilyIndex = queue;

		bufferBarrier.offset = offset;
		bufferBarrier.size = size;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.bufferMemoryBarrierCount = 1;
		depInfo.pBufferMemoryBarriers = &bufferBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void buffer_barrier_count(VkCommandBuffer cmd, BufferSyncData* buffers, uint32_t count, uint32_t queue, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
	{
		std::vector<VkBufferMemoryBarrier2> barriers;
		barriers.reserve(count);

		for (uint32_t i = 0; i < count; i++) {
			VkBufferMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr };
			barrier.buffer = buffers[i].buffer;

			barrier.srcStageMask = srcStage;
			barrier.srcAccessMask = srcAccess;
			barrier.dstStageMask = dstStage;
			barrier.dstAccessMask = dstAccess;

			barrier.srcQueueFamilyIndex = queue;
			barrier.dstQueueFamilyIndex = queue;

			barrier.offset = buffers[i].offset;
			barrier.size = buffers[i].size;

			barriers.push_back(barrier);
		}

		VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
		depInfo.bufferMemoryBarrierCount = count;
		depInfo.pBufferMemoryBarriers = barriers.data();

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	void debug_full_memory_barrier(VkCommandBuffer cmd, const char* text)
	{
		VkMemoryBarrier2 memoryBarrier{ .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		memoryBarrier.pNext = nullptr;

		memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.memoryBarrierCount = 1;
		depInfo.pMemoryBarriers = &memoryBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);

		if (text) {
			std::cout << "full pipeline barrier executed: " << text << std::endl;
		}
	}

}
