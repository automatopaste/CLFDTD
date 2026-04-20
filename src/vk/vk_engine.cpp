#include "vk_engine.h"

#include <SDL3/SDL_vulkan.h>

#include <vk/vk_initialisers.h>
#include <vk/vk_types.h>

#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vk/vk_images.h>
#include <vk/vk_pipelines.h>
#include <vk/vk_synchronisation.h>
#include <vk/vk_tex_store.h>

#include "r2/r2_commander.h"

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init() {
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        fmt::print("failed to initialise volk: vulkan loader not found");
        return;
    }

    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    _useValidation = false;
    _compileShaders = false;
    _compileAssetBinaries = false;

    resolutionScale = 1.0f;
    tempGUIResolutionScale = resolutionScale;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

#if _DEBUG
    constexpr auto LM_WINDOW_NAME = "FDTD 2D GPU Debug";
#else
    constexpr auto LM_WINDOW_NAME = "FDTD 2D GPU Release";
#endif

    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);

    if (count == 0) {
        fmt::print("SDL error: no valid displays\n");
    }

    SDL_DisplayID display = displays[0];
    SDL_free(displays);

    const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(display);
    if (!displayMode) {
        fmt::print("SDL error: {}\n", SDL_GetError());
    }

    _appUpdateRate = 120;
    _fpsCap = (uint32_t)displayMode->refresh_rate == 0 ? 60u : displayMode->refresh_rate;

    _windowExtent = VkExtent2D{ (uint32_t)displayMode->w, (uint32_t)displayMode->h };
    vsync = false;

    _window = SDL_CreateWindow(
        LM_WINDOW_NAME,
        _windowExtent.width,
        _windowExtent.height,
        window_flags
    );

    init_filesystem();

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_shaderc();

    init_imgui();

    init_default_data();

    compile_shaders();

    init_gui();

    init_renderer();

    frameTimes.resize(frameTimeSize);
    updatesPerFrame.resize(updatesPerFrameSize);
    updateTimes.resize(updatesPerFrameSize);

    // everything went fine
    _isInitialised = true;
}

void VulkanEngine::cleanup() {
    if (_isInitialised) {
        //make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_device);

        R2::cleanup(renderData, this);

        for (auto& frame : _frames) {
            frame._deletionQueue.flush();
        }

        _mainDeletionQueue.flush();

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        std::vector<uint32_t> undeallocatedImages;
        for (uint32_t i : imagesAllocated) {
            if (!imagesDeallocated.contains(i)) {
                undeallocatedImages.push_back(i);
            }
        }
        if (undeallocatedImages.size() > 0) {
            std::string formatted;
            uint32_t j{ 0 };
            for (uint32_t i : undeallocatedImages) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d", i);
                formatted.append(buf);
                if (j < undeallocatedImages.size() - 1) {
                    formatted.append(", ");
                }
                j++;
            }
            fmt::print("{} engine images not deallocated: [{}]\n", undeallocatedImages.size(), formatted);
        }
        else {
            fmt::print("all engine images deallocated\n");
        }

        std::vector<uint32_t> undeallocatedBuffers;
        for (uint32_t i : buffersAllocated) {
            if (!buffersDeallocated.contains(i)) {
                undeallocatedBuffers.push_back(i);
            }
        }
        if (undeallocatedBuffers.size() > 0) {
            std::string formatted;
            uint32_t j{ 0 };
            for (uint32_t i : undeallocatedBuffers) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d", i);
                formatted.append(buf);
                if (j < undeallocatedBuffers.size() - 1) {
                    formatted.append(", ");
                }
                j++;
            }
            fmt::print("{} engine buffers not deallocated: [{}]\n", undeallocatedBuffers.size(), formatted);
        }
        else {
            fmt::print("all engine buffers deallocated\n");
        }

        vmaDestroyAllocator(_allocator);

        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::run() {
    SDL_Event e;
    bool bQuit = false;

    long long fpsCapAccumulator = 0;
    long long appUpdateAccumulator = 0;

    auto current = std::chrono::high_resolution_clock::now();

    // main loop
    while (!bQuit && !appRequestedQuit) {
        auto start = std::chrono::high_resolution_clock::now();

        long long fpsCapDelta = static_cast<long long>(1000000000.0 / _fpsCap);

        long long timeDelta = static_cast<long long>(1000000000.0 / _appUpdateRate);
        float updateDelta = 1.0f / _appUpdateRate;

        long long frameTime = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(start - current).count());
        //frameTime = std::min(250000000, frameTime);
        current = start;

        frameTimes[frameTimeIndex] = static_cast<float>(frameTime * (1.0 / 1000000000));
        frameTimeIndex++;
        frameTimeIndex %= frameTimeSize;
        avgftime = std::reduce(frameTimes.begin(), frameTimes.end()) / static_cast<float>(frameTimeSize);

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // fix update rate to framerate
        /*auto g1 = std::chrono::high_resolution_clock::now();

        app_update(dt);

        auto g2 = std::chrono::high_resolution_clock::now();
        _msPerUpdate = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(g2 - g1).count());*/

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        std::vector<SDL_Event> events;
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_EVENT_QUIT) {
                bQuit = true;
            }

            switch (e.type) {
            case SDL_EVENT_WINDOW_MINIMIZED:
                stop_rendering = true;
                break;

            case SDL_EVENT_WINDOW_RESTORED:
                stop_rendering = false;
                _resizeRequested = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                _resizeRequested = true;
                break;
            default:
                break;
            }

            bool consumed = true; // leave for now
            bool imguiConsumed = ImGui_ImplSDL3_ProcessEvent(&e);

            if (!consumed) {
                events.push_back(e);
            }
        }

        float dt2 = static_cast<float>(frameTime * (1.0 / 1000000000));

        appUpdateAccumulator += frameTime;

        // cap accumulator to 10 updates
        appUpdateAccumulator = std::min(appUpdateAccumulator, timeDelta * 10);
        
        // skip render frames to maintain simulation speed
        int updates = 0;
        _msPerUpdate = 0;
        float t = 0.0f;
        while (appUpdateAccumulator >= timeDelta) {
            auto g1 = std::chrono::high_resolution_clock::now();

            // app update here
            
            auto g2 = std::chrono::high_resolution_clock::now();
            float updateTime = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(g2 - g1).count()) * 0.001f;
            t += updateTime;

            updateTimes[updateTimesIndex] = updateTime;
            updateTimesIndex++;
            updateTimesIndex %= updatesPerFrameSize;

            appUpdateAccumulator -= timeDelta;
            updates++;
        }

        updatesPerFrame[updatesPerFrameIndex] = updates;
        updatesPerFrameIndex++;
        updatesPerFrameIndex %= updatesPerFrameSize;
        _updatesPerFrame = std::reduce(updatesPerFrame.begin(), updatesPerFrame.end()) / static_cast<float>(updatesPerFrameSize);

        _msPerUpdate = std::reduce(updateTimes.begin(), updateTimes.end()) / static_cast<float>(updatesPerFrameSize);
        
        auto d1 = std::chrono::high_resolution_clock::now();

        if (ImGui::Begin("Engine Settings")) {
            int fpsCap = _fpsCap;
            int updateRate = _appUpdateRate;
            ImGui::SliderInt("fps cap", &fpsCap, 3, 165);
            ImGui::SliderInt("update rate", &updateRate, 3, 165);
            ImGui::SliderFloat("resolution scale", &tempGUIResolutionScale, 0.1f, 2.0f);
            if (ImGui::Button("apply res")) {
                resolutionScale = tempGUIResolutionScale;
                _resizeRequested = true;
            }
            if (ImGui::Button("compile shaders")) {
                _resizeRequested = true;
                _compileShaders = true;

#ifndef NDEBUG
                system("call ../../batch_asset_copy_debug.bat");
#else
                system("call ../../batch_asset_copy_release.bat");
#endif

                init_filesystem();
            }

            _fpsCap = static_cast<uint32_t>(fpsCap);
            _appUpdateRate = static_cast<uint32_t>(updateRate);
        }
        ImGui::End();

        draw();

        auto d2 = std::chrono::high_resolution_clock::now();
        _msPerFrame = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(d2 - d1).count()) / 1000.0f;

        // fps cap
        fpsCapAccumulator += fpsCapDelta;

        auto c1 = std::chrono::high_resolution_clock::now();
        long long currFrameDuration = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - start).count());
        fpsCapAccumulator -= currFrameDuration;

        auto w1 = std::chrono::high_resolution_clock::now();
        if (fpsCapAccumulator > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(fpsCapAccumulator));
        }
        auto w2 = std::chrono::high_resolution_clock::now();

        long long sleepDuration = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(w2 - w1).count());
        fpsCapAccumulator -= sleepDuration;

        _msIdlePerFrame = static_cast<float>(sleepDuration) / 1000000.0f;

        if (_resizeRequested) {
            resize_swapchain();
        }
    }
}

void VulkanEngine::quit()
{
    appRequestedQuit = true;
}

void VulkanEngine::draw() {
    auto d1 = std::chrono::high_resolution_clock::now();

    uint32_t frameIndex = _frameNumber % FRAME_OVERLAP;

    FrameData& frameData = get_current_frame();

    //wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));

    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_device);

    //request image from the swapchain
    uint32_t swapchainImageIndex;

    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, frameData.waitSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        _resizeRequested = true;
    }

    // wait on upload timeline semaphore
    SwapchainUploadSemaphores& swapchainUploadSemaphores = uploadSemaphores[swapchainImageIndex];

    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition swapchain source image for writing
    vkutil::image_barrier(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // renderer

    auto start = std::chrono::system_clock::now();

    // main renderer

    VkSemaphoreWaitInfo uploadWaitInfo = vkinit::semaphore_wait_info(&swapchainUploadSemaphores.uploadSemaphore, &swapchainUploadSemaphores.value);
    vkWaitSemaphores(_device, &uploadWaitInfo, 1000000000);

    R2::render(cmd, renderData, this, frameIndex, _swapchainImages[swapchainImageIndex], _swapchainImageViews[swapchainImageIndex]);

    // advance upload timeline semaphore
    swapchainUploadSemaphores.value++;
    VkSemaphoreSubmitInfo r2SignalInfo = vkinit::timeline_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, swapchainUploadSemaphores.uploadSemaphore, swapchainUploadSemaphores.value);
    signal_timeline_semaphore(r2SignalInfo);
    
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    _msMeshDraw = elapsed.count() / 1000.0f;

    // swapchain image format must be VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL here

    ImGui::Render();

    //draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainImages[swapchainImageIndex], _swapchainImageViews[swapchainImageIndex]);

    // transition swapchain source image for writing
    vkutil::image_barrier(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    vkutil::memory_barrier(
        cmd,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
    );

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue. 
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, swapchainUploadSemaphores.presentSemaphore);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frameData.waitSemaphore);

    // submit

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // add semaphore signals

    std::vector<VkSemaphoreSubmitInfo> semaphoreSignalSubmits;
    semaphoreSignalSubmits.push_back(signalInfo);

    for (auto& signal : timelineSemaphoreSignals) {
        semaphoreSignalSubmits.push_back(signal);
    }
    timelineSemaphoreSignals.clear();

    submit.pSignalSemaphoreInfos = semaphoreSignalSubmits.data();
    submit.signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignalSubmits.size());

    // add semaphore waits

    std::vector<VkSemaphoreSubmitInfo> semaphoreWaitSubmits;
    semaphoreWaitSubmits.push_back(waitInfo);

    for (auto& wait : timelineSemaphoreWaits) {
        semaphoreWaitSubmits.push_back(wait);
    }
    timelineSemaphoreWaits.clear();

    submit.pWaitSemaphoreInfos = semaphoreWaitSubmits.data();
    submit.waitSemaphoreInfoCount = static_cast<uint32_t>(semaphoreWaitSubmits.size());

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = vkinit::present_info();

    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &swapchainUploadSemaphores.presentSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        _resizeRequested = true;
    }

    //increase the number of frames drawn
    _frameNumber++;

    auto d2 = std::chrono::high_resolution_clock::now();
    _msRenderTime = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(d2 - d1).count()) / 1000.0f;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImage targetImage, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::init_filesystem()
{
    // create binary directories if they don't exist
    std::filesystem::path cwd = std::filesystem::current_path();

    std::filesystem::create_directory(cwd / "shaders");
    std::filesystem::create_directory(cwd / "shaders/bin");
    std::filesystem::create_directory(cwd / "assets");
    std::filesystem::create_directory(cwd / "assets/bin");
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder builder;

    //make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("LAZERMIND")
        .request_validation_layers(_useValidation)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    //grab the instance 
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    volkLoadInstance(_instance);

    SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface);

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = true;
    features.shaderInt64 = true;
    features.shaderInt16 = true;
    features.drawIndirectFirstInstance = true;
    features.multiDrawIndirect = true;
    features.geometryShader = true;

    //vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    features13.computeFullSubgroups = true;

    //vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.runtimeDescriptorArray = true;
    features12.scalarBlockLayout = true;
    features12.drawIndirectCount = true;
    features12.shaderOutputViewportIndex = true;
    features12.shaderOutputLayer = true;
    features12.shaderSampledImageArrayNonUniformIndexing = true;
    features12.hostQueryReset = true;
    features12.timelineSemaphore = true;
    features12.uniformAndStorageBuffer8BitAccess = true;

    VkPhysicalDeviceVulkan11Features features11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    features11.storageBuffer16BitAccess = true;
    features11.uniformAndStorageBuffer16BitAccess = true;
    features11.multiview = true;

    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    selector.set_minimum_version(1, 3);
    selector.set_required_features(features);
    selector.set_required_features_13(features13);
    selector.set_required_features_12(features12);
    selector.set_required_features_11(features11);
    selector.set_surface(_surface);
    selector.add_required_extension("VK_EXT_shader_viewport_index_layer");
    selector.add_required_extension("VK_KHR_spirv_1_4");
    selector.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);
    selector.allow_any_gpu_device_type(true);
    auto result = selector.select();
    if (!result.has_value()) {
        fmt::print("vk bootstrap error\n");
        auto s = selector.select().detailed_failure_reasons();
        for (auto& str : s) {
            fmt::print("{}\n", str);
        }
    }
    vkb::PhysicalDevice physicalDevice = result.value();

    //create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();
    std::string deviceName = vkbDevice.physical_device.name;
    fmt::print("selected device: {}\n", deviceName);

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _physicalDevice = physicalDevice.physical_device;

    volkLoadDevice(_device);

    _physicalDeviceProperties = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    _physicalDeviceSubgroupProperties = VkPhysicalDeviceSubgroupProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    _physicalDeviceRayTracingProperties = VkPhysicalDeviceRayTracingPipelinePropertiesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    _physicalDeviceAccelerationStructureProperties = VkPhysicalDeviceAccelerationStructurePropertiesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

    _physicalDeviceProperties.pNext = &_physicalDeviceRayTracingProperties;
    _physicalDeviceRayTracingProperties.pNext = &_physicalDeviceAccelerationStructureProperties;
    _physicalDeviceAccelerationStructureProperties.pNext = &_physicalDeviceSubgroupProperties;
    _physicalDeviceSubgroupProperties.pNext = nullptr;

    vkGetPhysicalDeviceProperties2(_physicalDevice, &_physicalDeviceProperties);

    // use vkbootstrap to get a Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaVulkanFunctions vmaVulkanFunctions{ 0 };
    vmaVulkanFunctions.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(_instance, "vkGetInstanceProcAddr");
    vmaVulkanFunctions.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetDeviceProcAddr(_device, "vkGetDeviceProcAddr");

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{ _physicalDevice, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VkPresentModeKHR presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    vkbSwapchainData = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(presentMode)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchainData.extent;
    //store swapchain and its related images
    _swapchain = vkbSwapchainData.swapchain;
    _swapchainImages = vkbSwapchainData.get_images().value();
    _swapchainImageViews = vkbSwapchainData.get_image_views().value();

    uploadSemaphores.resize(_swapchainImages.size());

    for (SwapchainUploadSemaphores& uploadSemaphore : uploadSemaphores) {
        {
            VkSemaphoreTypeCreateInfo typeCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            typeCreateInfo.initialValue = 0;
            VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            semaphoreCreateInfo.pNext = &typeCreateInfo;

            VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &uploadSemaphore.uploadSemaphore));
        }
        
        {
            VkSemaphoreTypeCreateInfo typeCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_BINARY;
            typeCreateInfo.initialValue = 0;
            VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            semaphoreCreateInfo.pNext = &typeCreateInfo;

            VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &uploadSemaphore.presentSemaphore));
        }
    }
}

void VulkanEngine::destroy_swapchain() {
    vkb::destroy_swapchain(vkbSwapchainData);

    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }

    for (SwapchainUploadSemaphores& uploadSemaphore : uploadSemaphores) {
        vkDestroySemaphore(_device, uploadSemaphore.uploadSemaphore, nullptr);
        vkDestroySemaphore(_device, uploadSemaphore.presentSemaphore, nullptr);
    }
    uploadSemaphores.clear();
}

void VulkanEngine::init_swapchain() {
    create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_commands() {
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = nullptr;
        cmdAllocInfo.commandPool = _frames[i]._commandPool;
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        _mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr); });
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
}

void VulkanEngine::init_sync_structures() {
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));

    _mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info(0);
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].waitSemaphore));

        _mainDeletionQueue.push_function([=]() {
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].waitSemaphore, nullptr);
        });
    }
}

void VulkanEngine::init_descriptors() {
    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }
    };
    descriptorAllocator.init(_device, 10, sizes);
    _mainDeletionQueue.push_function([&]() { 
        descriptorAllocator.destroy_pools(_device);
    });

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _mainDeletionQueue.push_function([&]() {
        vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
    });

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);

        _mainDeletionQueue.push_function([&, i]() {
            _frames[i]._frameDescriptors.destroy_pools(_device);
        });
    }

}

class _IncluderInterface : public shaderc::CompileOptions::IncluderInterface {
public:
    shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override {
        shaderc_include_result* result = new shaderc_include_result;

        std::filesystem::path cwd = std::filesystem::current_path();
        std::string requestedSourceName = requested_source;
        std::filesystem::path rspath = cwd / "shaders" / requestedSourceName;

        std::string s = rspath.string();
        
        const char* scstr = s.c_str();
        size_t scstrlen = strlen(scstr) + 1;
        char* rspathbuf = new char[scstrlen];
        strcpy_s(rspathbuf, scstrlen, scstr);

        result->source_name = rspathbuf;
        result->source_name_length = s.size();

        // open the file. With cursor at the end
        std::ifstream file(rspath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            fmt::print("{}", std::string("failed to open shader include file " + rspath.string() + "\n"));
            return nullptr;
        }

        size_t fileSize = (size_t)file.tellg();
        char* contentbuf = new char[fileSize];
        file.seekg(0);
        file.read(contentbuf, fileSize);
        file.close();

        result->content = contentbuf;
        result->content_length = fileSize;

        return result;
    }

    void ReleaseInclude(shaderc_include_result* data) override {
        delete[] data->source_name;
        delete[] data->content;
        delete data;
    }
};

void VulkanEngine::init_shaderc() {
    /*shadercCompiler = shaderc_compiler_initialize();

    shadercOptions = shaderc_compile_options_initialize();
    shaderc_compile_options_set_target_env(shadercOptions, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    shaderc_compile_options_set_target_spirv(shadercOptions, shaderc_spirv_version_1_4);
    shaderc_compile_options_set_optimization_level(shadercOptions, shaderc_optimization_level_performance);*/
    
    _compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    _compileOptions.SetTargetSpirv(shaderc_spirv_version_1_6);
    _compileOptions.SetOptimizationLevel(shaderc_optimization_level_performance);

    std::unique_ptr<shaderc::CompileOptions::IncluderInterface> includerInterface = std::make_unique<_IncluderInterface>();
    _compileOptions.SetIncluder(std::move(includerInterface));

#ifndef NDEBUG
    _compileOptions.SetGenerateDebugInfo();
#endif
}

void VulkanEngine::init_imgui() {
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // initialize imgui library

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL3_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _physicalDevice;
    init_info.Device = _device;
    init_info.QueueFamily = _graphicsQueueFamily;
    init_info.Queue = _graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = (uint32_t)_swapchainImages.size();
    init_info.ImageCount = (uint32_t)_swapchainImages.size();
    init_info.UseDynamicRendering = true;

    ImGui_ImplVulkan_PipelineInfo imguiPipelineInfo{};
    imguiPipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineRenderingCreateInfo imguiRenderCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    imguiRenderCreateInfo.colorAttachmentCount = 1;
    imguiRenderCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    imguiPipelineInfo.PipelineRenderingCreateInfo = imguiRenderCreateInfo;
    init_info.PipelineInfoMain = imguiPipelineInfo;

    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* fname, void* vkInstance) { return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vkInstance)), fname); }, &_instance);
    ImGui_ImplVulkan_Init(&init_info);

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}

void VulkanEngine::init_default_data() {
    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampl.pNext = nullptr;

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    sampl.anisotropyEnable = VK_TRUE;
    sampl.maxAnisotropy = _physicalDeviceProperties.properties.limits.maxSamplerAnisotropy;
    sampl.compareEnable = VK_FALSE;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampl.mipLodBias = 0.0f;
    sampl.minLod = 0.0f;
    sampl.maxLod = 13.0f;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    sampl.anisotropyEnable = VK_TRUE;
    sampl.maxAnisotropy = _physicalDeviceProperties.properties.limits.maxSamplerAnisotropy;
    sampl.compareEnable = VK_FALSE;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampl.mipLodBias = 0.0f;
    sampl.minLod = 0.0f;
    sampl.maxLod = 13.0f;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear); 

    _mainDeletionQueue.push_function([=, this]() {
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
    });
}

void VulkanEngine::init_gui()
{
}

void VulkanEngine::compile_shaders()
{
    
}

void VulkanEngine::init_renderer() {
    VkExtent2D drawExtent = VkExtent2D{
        .width = static_cast<uint32_t>(resolutionScale * _swapchainExtent.width),
        .height = static_cast<uint32_t>(resolutionScale * _swapchainExtent.height)
    };

    R2::init(renderData, this, _swapchainExtent, drawExtent);
}

void VulkanEngine::resize_swapchain() {
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    R2::cleanup(renderData, this);

    VkExtent2D drawExtent = VkExtent2D{
        .width = static_cast<uint32_t>(resolutionScale * _windowExtent.width),
        .height = static_cast<uint32_t>(resolutionScale * _windowExtent.height)
    };

    R2::init(renderData, this, _windowExtent, drawExtent);

    _resizeRequested = false;
    _compileShaders = false;
}

AllocatedBuffer VulkanEngine::create_buffer(size_t size, VkBufferUsageFlags usage, bool mapped) {
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = (VkDeviceSize)size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaallocInfo.flags = mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

    AllocatedBuffer newBuffer{};

    // allocate the buffer
    VK_CHECK(vmaCreateBufferWithAlignment(_allocator, &bufferInfo, &vmaallocInfo, BUFFER_ALIGNMENT, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newBuffer.buffer };
    newBuffer.address = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    /*if (newBuffer.address % 8 != 0) {
        fmt::print("misaligned buffer device address 0x{:x}\n", (uint64_t)newBuffer.address);
    }*/

    assert(newBuffer.address % 8 == 0);

    newBuffer.size = static_cast<uint32_t>(size);
    newBuffer.allocSize = static_cast<uint32_t>(bufferInfo.size);

    newBuffer.allocationIndex = bufferAllocIndex++;
    buffersAllocated.insert(newBuffer.allocationIndex);

    return newBuffer;
}

AllocatedBuffer VulkanEngine::create_buffer_staging(size_t size) {
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.size = (VkDeviceSize)size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    AllocatedBuffer newBuffer{};

    // allocate the buffer
    VK_CHECK(vmaCreateBufferWithAlignment(_allocator, &bufferInfo, &vmaallocInfo, BUFFER_ALIGNMENT, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    newBuffer.size = static_cast<uint32_t>(size);
    newBuffer.allocSize = static_cast<uint32_t>(bufferInfo.size);

    newBuffer.allocationIndex = bufferAllocIndex++;
    buffersAllocated.insert(newBuffer.allocationIndex);

    return newBuffer;
}

AllocatedBuffer VulkanEngine::create_buffer_aligned(size_t size, size_t alignment, VkBufferUsageFlags usage, bool mapped) {
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = (VkDeviceSize)size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaallocInfo.flags = mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

    AllocatedBuffer newBuffer{};

    // allocate the buffer
    VK_CHECK(vmaCreateBufferWithAlignment(_allocator, &bufferInfo, &vmaallocInfo, alignment, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newBuffer.buffer };
    newBuffer.address = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    if (newBuffer.address % 8 != 0) {
        fmt::print("misaligned buffer device address 0x{:x}\n", (uint64_t)newBuffer.address);
    }

    assert(newBuffer.address % 8 == 0);

    newBuffer.size = static_cast<uint32_t>(size);
    newBuffer.allocSize = static_cast<uint32_t>(bufferInfo.size);

    newBuffer.allocationIndex = bufferAllocIndex++;
    buffersAllocated.insert(newBuffer.allocationIndex);

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
    buffersDeallocated.insert(buffer.allocationIndex);
}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageCreateFlags create) {
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = vkutil::max_mip_levels(size);
    }
    img_info.flags = create;

    bool isCubemap = create & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (isCubemap) {
        img_info.arrayLayers = 6;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = img_info.mipLevels;
    view_info.subresourceRange.layerCount = img_info.arrayLayers;

    if (isCubemap) {
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    }

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    newImage.mipLevels = img_info.mipLevels;

    newImage.allocationIndex = imageAllocIndex++;
    imagesAllocated.insert(newImage.allocationIndex);

    createdImages.push_back(newImage.image);

    return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageAspectFlags aspect, VkImageCreateFlags create) {
    int numChannels = 4;
    switch (format) {
        case VK_FORMAT_R8_UNORM:
        {
            numChannels = 1;
            break;
        }
        case VK_FORMAT_R8G8_UNORM:
        {
            numChannels = 2;
            break;
        }
        case VK_FORMAT_R8G8B8A8_UNORM:
        {
            numChannels = 4;
            break;
        }
        default:
        {
            fmt::print("unknown channel count for format {}, fallback 4\n", string_VkFormat(format));
            numChannels = 4;
            break;
        }
    }

    size_t data_size = size.depth * size.width * size.height * numChannels;
    AllocatedBuffer uploadbuffer = create_buffer_staging(data_size);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, create);

    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspect);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{ new_image.imageExtent.width,new_image.imageExtent.height });
        }
        else {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aspect);
        }
    });

    destroy_buffer(uploadbuffer);

    return new_image;
}

void VulkanEngine::destroy_image(const AllocatedImage& img) {
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
    imagesDeallocated.insert(img.allocationIndex);
}

void VulkanEngine::set_camera(glm::mat4 camera, glm::vec4 cameraPos) {
    mainCamera = camera;
    mainCameraPos = cameraPos;
}

void VulkanEngine::submit_timeline_semaphore(VkSemaphoreSubmitInfo& signalInfo, VkSemaphoreSubmitInfo& waitInfo)
{
    timelineSemaphoreSignals.push_back(signalInfo);
    timelineSemaphoreWaits.push_back(waitInfo);
}

void VulkanEngine::signal_timeline_semaphore(VkSemaphoreSubmitInfo& signalInfo)
{
    timelineSemaphoreSignals.push_back(signalInfo);
}

void VulkanEngine::wait_timeline_semaphore(VkSemaphoreSubmitInfo& waitInfo)
{
    timelineSemaphoreWaits.push_back(waitInfo);
}



