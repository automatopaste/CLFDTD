#include "3d_sat_commander.h"

#include <thread>
#include <shared_mutex>

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

#include "../r2_config.h"

#include "3d_sat/3d_reset.h"
#include "3d_sat/3d_update.h"
#include "3d_sat/3d_output.h"

#include "imgui.h"

void sim_launch(const R2::RenderData renderData, VulkanEngine* engine, L3Dsat::UpdateData updateData, std::atomic_bool& exitFlag, std::atomic_bool& readFlag)
{
    fmt::print("Simulation thread launching...\n");

    std::array<VkCommandPool, FRAME_OVERLAP> commandPools;
    std::array<VkCommandBuffer, FRAME_OVERLAP> computeCmds;
    std::array<VkFence, FRAME_OVERLAP> computeFences;
    uint32_t frameIndex = 0;

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = renderData.computeQueueFamily;

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(engine->_device, &commandPoolInfo, nullptr, &commandPools[i]));

        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = nullptr;
        cmdAllocInfo.commandPool = commandPools[i];
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VK_CHECK(vkAllocateCommandBuffers(engine->_device, &cmdAllocInfo, &computeCmds[i]));
    }

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }
    };
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        updateData.descriptorAllocators[i].init(engine->_device, 10, sizes);
    }

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(engine->_device, &fenceCreateInfo, nullptr, &computeFences[i]));
    }

    bool exit{ false };
    while (!exit) {
        VK_CHECK(vkWaitForFences(engine->_device, 1, &computeFences[frameIndex], true, 1000000000));
        VK_CHECK(vkResetFences(engine->_device, 1, &computeFences[frameIndex]));

        // submit to dedicated compute queue
        VkCommandBuffer computeCmd = computeCmds[frameIndex];
        VK_CHECK(vkResetCommandBuffer(computeCmd, 0));
        VkCommandBufferBeginInfo computeCmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(computeCmd, &computeCmdBeginInfo));

        // update pass
        L3DsatUpdate::render(computeCmd, updateData, engine, frameIndex);

        VK_CHECK(vkEndCommandBuffer(computeCmd));
        VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(computeCmd);

        VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

        VK_CHECK(vkQueueSubmit2(renderData.computeQueue, 1, &submit, computeFences[frameIndex]));

        frameIndex = (frameIndex++) % FRAME_OVERLAP;
        if (updateData.step >= updateData.maxStep) {
            exit = true;
        }

        if (updateData.step % 100 == 0) {
            fmt::print("Simulation step: {}/{}\n", updateData.step, updateData.maxStep);
        }
    }

    vkDeviceWaitIdle(engine->_device);

    fmt::print("Simulation thread cleaning up...\n");

    // cleanup

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        vkDestroyCommandPool(engine->_device, commandPools[i], nullptr);
        vkDestroyFence(engine->_device, computeFences[i], nullptr);

        updateData.descriptorAllocators[i].destroy_pools(engine->_device);
    }

    exitFlag = true;
}

void L3Dsat::init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent)
{
    L3Dsat::Data* pData = new L3Dsat::Data();
    renderData.userData = pData;
    L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

    renderData.swapchainExtent = swapchainExtent;
    renderData.drawExtent = drawExtent;

    // attachment images

    VkExtent3D drawExtent3D = VkExtent3D{ .width = renderData.drawExtent.width, .height = renderData.drawExtent.height, .depth = 1 };

    VkImageUsageFlags drawImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    R2::create_image(engine, &renderData.drawImage, drawExtent3D, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsageFlags, false, 0, VK_IMAGE_ASPECT_COLOR_BIT);

    // descriptor allocators

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 }
        };
        renderData.descriptorAllocators[i].init(engine->_device, 10, sizes);
    }

    // init renderpasses

    L3DsatReset::init(renderData, engine);
    L3DsatUpdate::init(renderData, engine);
    L3DsatOutput::init(renderData, engine);

    // renderpass graph dependencies

    // attachment reset pass

    R2::RenderPass* resetRenderPass = &data.resetPass;
    R2::pass_init(
        resetRenderPass,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT
    );

    R2::pass_add_image_transition(resetRenderPass, &renderData.drawImage,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        0
    );

    // output

    R2::RenderPass* outputPass = &data.outputPass;
    R2::pass_init(outputPass,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );

    R2::pass_add_dependency(resetRenderPass, outputPass);

    R2::pass_add_image_transition(resetRenderPass, &renderData.drawImage,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        0
    );
}

void L3Dsat::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
    delete renderData.userData;

    R2::destroy_image(engine, renderData.drawImage);

    L3DsatReset::cleanup(renderData, engine);
    L3DsatUpdate::cleanup(renderData, engine);
    L3DsatOutput::cleanup(renderData, engine);

    std::vector<uint32_t> undeallocatedImages;
    for (uint32_t i : engine->r2ImageAllocations) {
        if (!engine->r2ImageDeallocations.contains(i)) {
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
        fmt::print("{} r2 images not deallocated: [{}]\n", undeallocatedImages.size(), formatted);
    }
    else {
        fmt::print("all r2 images deallocated\n");
    }

    std::vector<uint32_t> undeallocatedBuffers;
    for (uint32_t i : engine->r2BufferAllocations) {
        if (!engine->r2BufferDeallocations.contains(i)) {
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
        fmt::print("{} r2 buffers not deallocated: [{}]\n", undeallocatedBuffers.size(), formatted);
    }
    else {
        fmt::print("all r2 buffers deallocated\n");
    }

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        renderData.descriptorAllocators[i].destroy_pools(engine->_device);
    }
}

void L3Dsat::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView)
{
    L3Dsat::Data& data = renderData.get_user_data<L3Dsat::Data>();

    R2::debug_name_begin(cmd, "Init");

    R2::FrameData& frameData = renderData.frameData[frameIndex];
    frameData.swapchainImage = swapchainImage;
    frameData.swapchainImageView = swapchainImageView;

    renderData.descriptorAllocators[frameIndex].clear_pools(engine->_device);

    R2::debug_name_end(cmd);

    // main render passes

    R2::set_image_layout_undefined(&renderData.drawImage);

    // attachment reset pass
    R2::debug_name_begin(cmd, "Reset");
    L3DsatReset::render(cmd, renderData, engine, frameIndex);
    R2::pass_cmd_barrier(&data.resetPass, cmd);
    R2::debug_name_end(cmd);

    bool restart{ false };
    if (ImGui::Begin("Simulation Controls")) {
        restart = ImGui::Button("Run Simulation");
    }
    ImGui::End();

    if (restart) {
        data.simExitFlag = false;

        L3Dsat::UpdateData updateData = data.updateData;

        if (data.simThread.joinable()) {
            data.simThread.join();
        }

        data.simThread = std::thread(sim_launch, renderData, engine, updateData, std::ref(data.simExitFlag), std::ref(data.simReadFlag));
    }

    // output pass
    R2::debug_name_begin(cmd, "Output");
    L3DsatOutput::render(cmd, renderData, engine, frameIndex);
    R2::pass_cmd_barrier(&data.outputPass, cmd);
    R2::debug_name_end(cmd);
}