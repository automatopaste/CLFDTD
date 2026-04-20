#pragma once

#include "vk/vk_pipelines.h"
#include "vk/vk_descriptors.h"

#include "r2_pass_structs.h"

namespace R2 {

	struct FrameData {

		// reference to swapchain image
		VkImage swapchainImage;
		VkImageView swapchainImageView;

		// global data buffer
		BufferState globalBuffer;

	};

	struct RenderData {
		FrameData frameData[FRAME_OVERLAP];

		VkExtent2D swapchainExtent;
		VkExtent2D drawExtent;

		// global descriptor set for global data

		Descriptor globalDescriptor;
		Descriptor attachmentDescriptor;
		Descriptor postProcessingDescriptor;
		DescriptorAllocatorGrowable descriptorAllocators[FRAME_OVERLAP];

		// colour images

		ImageState drawImage;
		//ImageState postImage;

		// pass-specific data

		RenderPass resetPass;
		RenderPass outputPass;

		Output::Data outputData;
	};

}
