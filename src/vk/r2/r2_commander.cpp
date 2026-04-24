#include "r2_commander.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

#include "r2_config.h"

#include "passes/reset/r2_reset.h"
#include "passes/update/r2_update.h"
#include "passes/output/r2_output.h"

void R2::init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent)
{
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
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 }
        };
        renderData.descriptorAllocators[i].init(engine->_device, 10, sizes);
    }

    // init renderpasses

    Reset::init(renderData, engine);
    Update::init(renderData, engine);
    Output::init(renderData, engine);

    // renderpass graph dependencies

    // attachment reset pass

    RenderPass* resetRenderPass = &renderData.resetPass;
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

    RenderPass* updateRenderPass = &renderData.updatePass;
    R2::pass_init(
        updateRenderPass,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );

    R2::pass_add_dependency(resetRenderPass, updateRenderPass);

    // output

    RenderPass* outputPass = &renderData.outputPass;
    R2::pass_init(outputPass,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );

    R2::pass_add_dependency(updateRenderPass, outputPass);

    R2::pass_add_image_transition(updateRenderPass, &renderData.drawImage,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        0
    );
}

void R2::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
    R2::destroy_image(engine, renderData.drawImage);

    Reset::cleanup(renderData, engine);
    Update::cleanup(renderData, engine);
    Output::cleanup(renderData, engine);

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

void R2::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView)
{
    R2::debug_name_begin(cmd, "Init");

    FrameData& frameData = renderData.frameData[frameIndex];
    frameData.swapchainImage = swapchainImage;
    frameData.swapchainImageView = swapchainImageView;

    renderData.descriptorAllocators[frameIndex].clear_pools(engine->_device);

    R2::debug_name_end(cmd);

    // main render passes

    R2::set_image_layout_undefined(&renderData.drawImage);

    // attachment reset pass
    R2::debug_name_begin(cmd, "Reset");
    Reset::render(cmd, renderData, engine, frameIndex);
    R2::pass_cmd_barrier(&renderData.resetPass, cmd);
    R2::debug_name_end(cmd);

    // update pass
    R2::debug_name_begin(cmd, "Update");
    Update::render(cmd, renderData, engine, frameIndex);
    R2::pass_cmd_barrier(&renderData.updatePass, cmd);
    R2::debug_name_end(cmd);

    // output pass
    R2::debug_name_begin(cmd, "Output");
    Output::render(cmd, renderData, engine, frameIndex);
    R2::pass_cmd_barrier(&renderData.outputPass, cmd);
    R2::debug_name_end(cmd);
}
