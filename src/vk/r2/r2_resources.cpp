#include "r2_resources.h"

#include "vk/vk_engine.h"
#include "vk/vk_synchronisation.h"

void R2::create_image(VulkanEngine* engine, ImageState* dest, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageCreateFlags create, VkImageAspectFlags aspect)
{
    ImageState& image = *dest;
    image = ImageState{};
    image.image = engine->create_image(size, format, usage, mipmapped, create);
    image.aspect = aspect;

    image.allocationIndex = engine->r2ImageIndex++;
    engine->r2ImageAllocations.insert(image.allocationIndex);
}

void R2::destroy_image(VulkanEngine* engine, ImageState& image)
{
    engine->destroy_image(image.image);
    engine->r2ImageDeallocations.insert(image.allocationIndex);
}

void R2::image_barrier(ImageResource* image, VkCommandBuffer cmd)
{
    if (image->mipLevels > 0) {
        vkutil::image_barrier_mipchain_generate(cmd, image->image->image.image, image->filter, image->image->image.extent(), image->srcStage, image->dstStage, image->srcAccess, image->dstAccess, image->image->layout, image->dstLayout, image->image->aspect, 0, image->mipLevels);
    }
    else {
        vkutil::image_barrier(cmd, image->image->image.image, image->srcStage, image->dstStage, image->srcAccess, image->dstAccess, image->image->layout, image->dstLayout, image->image->aspect);
    }
}

void R2::create_image(VulkanEngine* engine, PingPongImage* dest, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageCreateFlags create, VkImageAspectFlags aspect)
{
    PingPongImage& image = *dest;
    image = PingPongImage{};
    image.image0 = engine->create_image(size, format, usage, mipmapped, create);
    image.image1 = engine->create_image(size, format, usage, mipmapped, create);
}

void R2::destroy_image(VulkanEngine* engine, PingPongImage& image)
{
    engine->destroy_image(image.image0);
    engine->destroy_image(image.image1);
}

void R2::create_buffer(VulkanEngine* engine, BufferState* dest, size_t size, VkBufferUsageFlags usage, bool mapped, uint32_t alignment)
{
    BufferState& buffer = *dest;
    buffer = BufferState{};
    buffer.currAccess = VK_ACCESS_2_NONE;
    buffer.currStage = VK_PIPELINE_STAGE_2_NONE;

    // allocate buffer
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaallocInfo.flags = mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

    // allocate the buffer
    VK_CHECK(vmaCreateBufferWithAlignment(engine->_allocator, &bufferInfo, &vmaallocInfo, alignment, &buffer.buffer, &buffer.allocation, &buffer.info));

    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.buffer };
    buffer.address = vkGetBufferDeviceAddress(engine->_device, &deviceAddressInfo);

    buffer.size = static_cast<uint32_t>(size);

    buffer.allocationIndex = engine->r2BufferIndex++;
    engine->r2BufferAllocations.insert(buffer.allocationIndex);
}

void R2::destroy_buffer(VulkanEngine* engine, BufferState& buffer)
{
    vmaDestroyBuffer(engine->_allocator, buffer.buffer, buffer.allocation);
    engine->r2BufferDeallocations.insert(buffer.allocationIndex);
    buffer = BufferState{};
}

void R2::create_buffer(VulkanEngine* engine, DynamicUploadBufferState* dest)
{
    *dest = DynamicUploadBufferState{};
    dest->allocationIndex = engine->r2BufferIndex++;
    engine->r2BufferAllocations.insert(dest->allocationIndex);
}

void R2::destroy_buffer(VulkanEngine* engine, DynamicUploadBufferState& buffer)
{
    buffer.buffer.deallocate(engine);
    engine->r2BufferDeallocations.insert(buffer.allocationIndex);
}

void R2::create_buffer(VulkanEngine* engine, DynamicBufferState* dest, VkBufferUsageFlags usage)
{
    *dest = DynamicBufferState{};
    dest->buffer.alloc(8, engine, usage);
    dest->allocationIndex = engine->r2BufferIndex++;
    engine->r2BufferAllocations.insert(dest->allocationIndex);
}

void R2::destroy_buffer(VulkanEngine* engine, DynamicBufferState& buffer)
{
    buffer.buffer.destroy(engine);
    engine->r2BufferDeallocations.insert(buffer.allocationIndex);
}

void R2::destroy_descriptor(VulkanEngine* engine, Descriptor& descriptor)
{
    vkDestroyDescriptorSetLayout(engine->_device, descriptor.layout, nullptr);
}

void R2::destroy_pipeline(VulkanEngine* engine, Pipeline& pipeline)
{
    vkDestroyPipeline(engine->_device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(engine->_device, pipeline.layout, nullptr);
}

void R2::create_semaphore(VulkanEngine* engine, Semaphore* dest, uint64_t initialValue)
{
    VkSemaphoreTypeCreateInfo typeCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeCreateInfo.initialValue = 0;
    VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    semaphoreCreateInfo.pNext = &typeCreateInfo;

    vkCreateSemaphore(engine->_device, &semaphoreCreateInfo, nullptr, &dest->semaphore);
}

void R2::signal_semaphore(VulkanEngine* engine, Semaphore& semaphore, VkPipelineStageFlags2 stage, uint64_t value)
{
    VkSemaphoreSubmitInfo submitInfo = vkinit::timeline_submit_info(stage, semaphore.semaphore, value);
    engine->signal_timeline_semaphore(submitInfo);
}

void R2::wait_semaphore(VulkanEngine* engine, Semaphore& semaphore, uint64_t value)
{
    VkSemaphoreWaitInfo waitInfo = vkinit::semaphore_wait_info(&semaphore.semaphore, &value);
    vkWaitSemaphores(engine->_device, &waitInfo, 1000000000);
}

void R2::destroy_semaphore(VulkanEngine* engine, Semaphore& semaphore)
{
    vkDestroySemaphore(engine->_device, semaphore.semaphore, nullptr);
}

void R2::pass_init(RenderPass* dest, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 srcStage, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstStage)
{
    RenderPass& renderPass = *dest;
    renderPass = RenderPass{};
    renderPass.srcAccess = srcAccess;
    renderPass.srcStage = srcStage;
    renderPass.dstAccess = dstAccess;
    renderPass.dstStage = dstStage;
}

void R2::pass_add_dependency(RenderPass* src, RenderPass* dst)
{
    RenderPass& srcPass = *src;
    RenderPass& dstPass = *dst;
    srcPass.dependentAccess |= dstPass.srcAccess;
    srcPass.dependentStage |= dstPass.srcStage;
}

void R2::set_image_layout_undefined(ImageState* image)
{
    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void R2::pass_add_image_transition(RenderPass* src, ImageState* image, VkImageLayout layout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, uint32_t mipLevels, VkFilter filter)
{
    ImageResource& imageResource = src->imageResources.emplace_back();
    imageResource.image = image;
    imageResource.dstLayout = layout;
    imageResource.srcStage = srcStage;
    imageResource.dstStage = dstStage;
    imageResource.srcAccess = srcAccess;
    imageResource.dstAccess = dstAccess;
    imageResource.mipLevels = mipLevels;
    imageResource.filter = filter;
}

void R2::pass_cmd_barrier(RenderPass* renderPass, VkCommandBuffer cmd)
{
    vkutil::memory_barrier(cmd, renderPass->dstStage, renderPass->dependentStage, renderPass->dstAccess, renderPass->dependentAccess);

    for (auto& imageResource : renderPass->imageResources) {
        image_barrier(&imageResource, cmd);
    }
}

void R2::destroy_sbt(VulkanEngine* engine, ShaderBindingTable& sbt)
{
    R2::destroy_buffer(engine, sbt.buffer);
}

void R2::ShaderBindingTable::push_group(VkShaderModule shader, VkShaderStageFlagBits stage)
{
    uint32_t stageIndex = (uint32_t)stageCreateInfos.size();
    uint32_t groupIndex = (uint32_t)groupCreateInfos.size();

    VkRayTracingShaderGroupCreateInfoKHR& groupCreateInfo = groupCreateInfos.emplace_back();
    groupCreateInfo = VkRayTracingShaderGroupCreateInfoKHR{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };

    groupCreateInfo.generalShader = VK_SHADER_UNUSED_KHR;
    groupCreateInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    switch (stage) {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
    {
        groupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groupCreateInfo.generalShader = stageIndex;

        raygenData.groupIndices.push_back(groupIndex);
        break;
    }
    case VK_SHADER_STAGE_MISS_BIT_KHR:
    {
        groupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groupCreateInfo.generalShader = stageIndex;

        missData.groupIndices.push_back(groupIndex);
        break;
    }
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
    {
        groupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        groupCreateInfo.closestHitShader = stageIndex;

        hitData.groupIndices.push_back(groupIndex);
        break;
    }
    }

    VkPipelineShaderStageCreateInfo& stageCreateInfo = stageCreateInfos.emplace_back();
    stageCreateInfo = VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stageCreateInfo.stage = stage;
    stageCreateInfo.module = shader;
    stageCreateInfo.pName = "main";
}

VkPipeline R2::ShaderBindingTable::build_pipeline(VulkanEngine* engine, VkPipelineLayout layout, uint32_t maxRecursionDepth)
{
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    pipelineInfo.stageCount = (uint32_t)stageCreateInfos.size();
    pipelineInfo.pStages = stageCreateInfos.data();
    pipelineInfo.groupCount = (uint32_t)groupCreateInfos.size();
    pipelineInfo.pGroups = groupCreateInfos.data();
    pipelineInfo.maxPipelineRayRecursionDepth = maxRecursionDepth;
    pipelineInfo.layout = layout;

    VkPipeline pipeline;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(engine->_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    return pipeline;
}

static inline uint32_t align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

void R2::ShaderBindingTable::build_table(VulkanEngine* engine, VkPipeline pipeline)
{
    uint32_t handleSize = engine->_physicalDeviceRayTracingProperties.shaderGroupHandleSize;
    uint32_t handleAlignment = engine->_physicalDeviceRayTracingProperties.shaderGroupHandleAlignment;
    uint32_t baseAlignment = engine->_physicalDeviceRayTracingProperties.shaderGroupBaseAlignment;

    if (!raygenData.groupIndices.empty()) {
        raygenData.strided.stride = (VkDeviceSize)align_up(handleSize + raygenData.dataSize, baseAlignment); // special case alignment
        raygenData.strided.size = raygenData.strided.stride;
    }

    if (!missData.groupIndices.empty()) {
        missData.strided.stride = (VkDeviceSize)align_up(handleSize + missData.dataSize, handleAlignment);
        missData.strided.size = (VkDeviceSize)align_up((uint32_t)missData.groupIndices.size() * (uint32_t)missData.strided.stride, handleAlignment);
    }

    if (!hitData.groupIndices.empty()) {
        hitData.strided.stride = (VkDeviceSize)align_up(handleSize + hitData.dataSize, handleAlignment);
        hitData.strided.size = (VkDeviceSize)align_up((uint32_t)hitData.groupIndices.size() * (uint32_t)hitData.strided.stride, handleAlignment);
    }

    if (!callableData.groupIndices.empty()) {
        callableData.strided.stride = (VkDeviceSize)align_up(handleSize + callableData.dataSize, handleAlignment);
        callableData.strided.size = (VkDeviceSize)align_up((uint32_t)callableData.groupIndices.size() * (uint32_t)callableData.strided.stride, handleAlignment);
    }

    uint32_t totalSize = 
        align_up((uint32_t)raygenData.strided.size, baseAlignment) +
        align_up((uint32_t)missData.strided.size, baseAlignment) +
        align_up((uint32_t)hitData.strided.size, baseAlignment) +
        align_up((uint32_t)callableData.strided.size, baseAlignment);

    create_buffer(engine, &buffer, totalSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, true, baseAlignment);

    uint32_t shaderGroupHandleSize = engine->_physicalDeviceRayTracingProperties.shaderGroupHandleSize;

    std::vector<uint8_t> groupHandles;
    size_t dataSize = groupCreateInfos.size() * shaderGroupHandleSize;
    groupHandles.resize(dataSize);

    vkGetRayTracingShaderGroupHandlesKHR(engine->_device, pipeline, 0, (uint32_t)groupCreateInfos.size(), dataSize, (void*)groupHandles.data());

    VkDeviceAddress baseAddress = buffer.address;

    size_t writeByteOffset = 0;
    char* pData = (char*)groupHandles.data();

    if (!raygenData.groupIndices.empty()) {
        raygenData.strided.deviceAddress = baseAddress + writeByteOffset;
    }

    for (uint32_t groupIndex : raygenData.groupIndices) {
        memcpy(
            (char*)buffer.info.pMappedData + writeByteOffset,
            pData + (groupIndex * shaderGroupHandleSize),
            shaderGroupHandleSize
        );

        writeByteOffset += raygenData.strided.stride;
    }
    writeByteOffset = align_up((uint32_t)writeByteOffset, baseAlignment);

    if (!missData.groupIndices.empty()) {
        missData.strided.deviceAddress = baseAddress + writeByteOffset;
    }

    for (uint32_t groupIndex : missData.groupIndices) {
        memcpy(
            (char*)buffer.info.pMappedData + writeByteOffset,
            pData + (groupIndex * shaderGroupHandleSize),
            shaderGroupHandleSize
        );

        writeByteOffset += missData.strided.stride;
    }
    writeByteOffset = align_up((uint32_t)writeByteOffset, baseAlignment);

    if (!hitData.groupIndices.empty()) {
        hitData.strided.deviceAddress = baseAddress + writeByteOffset;
    }

    for (uint32_t groupIndex : hitData.groupIndices) {
        memcpy(
            (char*)buffer.info.pMappedData + writeByteOffset,
            pData + (groupIndex * shaderGroupHandleSize),
            shaderGroupHandleSize
        );

        writeByteOffset += hitData.strided.stride;
    }
    writeByteOffset = align_up((uint32_t)writeByteOffset, baseAlignment);

    if (!callableData.groupIndices.empty()) {
        callableData.strided.deviceAddress = baseAddress + writeByteOffset;
    }

    for (uint32_t groupIndex : callableData.groupIndices) {
        memcpy(
            (char*)buffer.info.pMappedData + writeByteOffset,
            pData + (groupIndex * shaderGroupHandleSize),
            shaderGroupHandleSize
        );

        writeByteOffset += callableData.strided.stride;
    }
    writeByteOffset = align_up((uint32_t)writeByteOffset, baseAlignment);

    assert(writeByteOffset == buffer.size);
}

R2::ComputeBuilder R2::compute_builder(VulkanEngine* engine)
{
    return ComputeBuilder{ engine };
}

void R2::debug_name_begin(VkCommandBuffer cmd, const char* name)
{
    VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

void R2::debug_name_end(VkCommandBuffer cmd)
{
    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void R2::ComputeBuilder::descriptors(VkDescriptorSetLayout* layouts, uint32_t count)
{
    pipelineLayoutCreateInfo.pSetLayouts = layouts;
    pipelineLayoutCreateInfo.setLayoutCount = count;
}

void R2::ComputeBuilder::push_constants(size_t size, VkShaderStageFlags stage)
{
    pushConstantRange.offset = 0;
    pushConstantRange.size = (uint32_t)size;
    pushConstantRange.stageFlags = stage;
}

void R2::ComputeBuilder::spec_const(uint32_t value, size_t size)
{
    specConstantData.resize(specConstantData.size() + size);
    uint8_t* pData = specConstantData.data() + specConstantOffset;
    memcpy(pData, &value, size);

    uint32_t id = (uint32_t)specConstants.size();

    VkSpecializationMapEntry mapEntry{
        .constantID = id,
        .offset = specConstantOffset,
        .size = size
    };
    specConstants.push_back(mapEntry);

    specConstantOffset += (uint32_t)size;
}

void R2::ComputeBuilder::clear_spec_const()
{
    specConstantOffset = 0;
    specConstants.clear();
    specConstantData.clear();
}

void R2::ComputeBuilder::set_shader_flags(VkPipelineShaderStageCreateFlags flags)
{
    createFlags = flags;
}

void R2::ComputeBuilder::shader(const char* string)
{
    // force recompile if spec constants are used
    vkutil::check_compile_load(string, engine, &shaderModule, engine->_compileShaders || !specConstants.empty());
}

void R2::ComputeBuilder::build(R2::Pipeline* dest)
{
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &pipelineLayoutCreateInfo, nullptr, &dest->layout));

    ComputePipelineBuilder computePipelineBuilder{};

    if (!specConstants.empty()) {
        VkSpecializationInfo specInfo{};
        specInfo.dataSize = specConstantOffset;
        specInfo.mapEntryCount = (uint32_t)specConstants.size();
        specInfo.pData = specConstantData.data();
        specInfo.pMapEntries = specConstants.data();

        computePipelineBuilder.set_spec_constants(specInfo);
    }

    computePipelineBuilder.set_shader(shaderModule);
    computePipelineBuilder.set_pipeline_layout(dest->layout);
    computePipelineBuilder.build(engine->_device, dest->pipeline);

    vkDestroyShaderModule(engine->_device, shaderModule, nullptr);
}
