#pragma once

#include <unordered_set>

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>

#include "VkBootstrap.h"

#include "SDL3/SDL.h"

#include <vk/vk_types.h>
#include <vk/vk_descriptors.h>

#include "vk/r2/r2_structs.h"

#define SHADERC_IMPLEMENTATION

struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	// binary semaphore for each frame
	VkSemaphore waitSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
};

struct SwapchainUploadSemaphores {
	// timeline semaphore that blocks upload operations
	VkSemaphore uploadSemaphore{};
	uint64_t value{ 0 };
	// binary present semaphore for each swapchain image
	VkSemaphore presentSemaphore{};
};

class VulkanEngine {
public:

	bool _useValidation;
	bool _compileShaders;

	uint32_t _appUpdateRate{ 1 };
	uint32_t _fpsCap{ 999 };

	bool vsync = true;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	bool _isInitialised{ false };
	uint64_t _frameNumber{ 0 };
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1920 , 1080 };

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _physicalDevice;// GPU chosen as the default device
	VkPhysicalDeviceProperties2 _physicalDeviceProperties;
	VkPhysicalDeviceSubgroupProperties _physicalDeviceSubgroupProperties;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR _physicalDeviceRayTracingProperties;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR _physicalDeviceAccelerationStructureProperties;
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	vkb::Swapchain vkbSwapchainData;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VmaAllocator _allocator;

	DeletionQueue _mainDeletionQueue;

	// draw resources
	float resolutionScale = 1.0f;
	float tempGUIResolutionScale = 1.0f;

	DescriptorAllocatorGrowable descriptorAllocator;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// shaderc compiler
	//shaderc_compiler_t shadercCompiler;
	//shaderc_compile_options_t shadercOptions;
	shaderc::CompileOptions _compileOptions{};
	shaderc::Compiler _compiler{};

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	glm::mat4 mainCamera;
	glm::vec4 mainCameraPos;

	// update timing
	float _msPerUpdate{ 0.0f };
	float _msPerFrame{ 0.0f };
	float _msIdlePerFrame{ 0.0f };
	float _msMeshDraw{ 0.0f };
	float _msRenderTime{ 0.0f };
	float _updatesPerFrame{ 0 };

	std::vector<float> frameTimes;
	float avgftime;
	int frameTimeIndex{ 0 };
	const int frameTimeSize = 15;

	std::vector<int> updatesPerFrame;
	int updatesPerFrameIndex{ 0 };
	const int updatesPerFrameSize = 60;
	std::vector<float> updateTimes;
	int updateTimesIndex{ 0 };

	uint32_t bufferAllocIndex{ 0 };
	uint32_t imageAllocIndex{ 0 };
	std::unordered_set<uint32_t> buffersAllocated;
	std::unordered_set<uint32_t> buffersDeallocated;
	std::unordered_set<uint32_t> imagesAllocated;
	std::unordered_set<uint32_t> imagesDeallocated;

	// default data

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	VkDescriptorSetLayout _singleImageDescriptorLayout;

	// forward+ renderer
	R2::RenderData renderData;
	uint32_t r2ImageIndex{ 0 };
	uint32_t r2BufferIndex{ 0 };
	std::unordered_set<uint32_t> r2ImageAllocations;
	std::unordered_set<uint32_t> r2ImageDeallocations;
	std::unordered_set<uint32_t> r2BufferAllocations;
	std::unordered_set<uint32_t> r2BufferDeallocations;

	bool _compileAssetBinaries{ false };

	bool _resizeRequested{ false };

	std::vector<SwapchainUploadSemaphores> uploadSemaphores;

	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	bool appRequestedQuit{ false };
	void quit();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	AllocatedBuffer create_buffer(size_t size, VkBufferUsageFlags usage, bool mapped);
	AllocatedBuffer create_buffer_staging(size_t size);
	AllocatedBuffer create_buffer_aligned(size_t size, size_t alignment, VkBufferUsageFlags usage, bool mapped);
	void destroy_buffer(const AllocatedBuffer& buffer);

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageCreateFlags create);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageAspectFlags aspect, VkImageCreateFlags create = 0);
	void destroy_image(const AllocatedImage& img);

	void set_camera(glm::mat4 camera, glm::vec4 cameraPos);

	std::vector<VkSemaphoreSubmitInfo> timelineSemaphoreSignals;
	std::vector<VkSemaphoreSubmitInfo> timelineSemaphoreWaits;
	void submit_timeline_semaphore(VkSemaphoreSubmitInfo& signalInfo, VkSemaphoreSubmitInfo& waitInfo);
	void signal_timeline_semaphore(VkSemaphoreSubmitInfo& signalInfo);
	void wait_timeline_semaphore(VkSemaphoreSubmitInfo& waitInfo);

private:

	std::vector<VkImage> createdImages;

	void init_filesystem();
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_shaderc();
	void init_imgui();
	void init_default_data();
	void init_gui();
	void compile_shaders();
	void init_renderer();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void resize_swapchain();

	void draw_imgui(VkCommandBuffer cmd, VkImage targetImage, VkImageView targetImageView);

};