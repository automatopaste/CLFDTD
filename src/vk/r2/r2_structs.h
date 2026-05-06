#pragma once

#include "vk/vk_pipelines.h"
#include "vk/vk_descriptors.h"

#include "r2_pass_structs.h"
#include "r2_resources.h"

namespace R2 {

	struct FrameData {
		// reference to swapchain image
		VkImage swapchainImage;
		VkImageView swapchainImageView;
	};

	struct RenderData {
		VkQueue computeQueue;
		uint32_t computeQueueFamily;

		FrameData frameData[FRAME_OVERLAP];

		VkExtent2D swapchainExtent;
		VkExtent2D drawExtent;

		// global descriptor set for global data

		Descriptor attachmentDescriptor;
		Descriptor postProcessingDescriptor;
		DescriptorAllocatorGrowable descriptorAllocators[FRAME_OVERLAP];

		// colour images

		ImageState drawImage;
		//ImageState postImage;

		void* userData;

		template <typename T>
		inline T& get_user_data() const {
			return *static_cast<T*>(userData);
		}
	};

}
