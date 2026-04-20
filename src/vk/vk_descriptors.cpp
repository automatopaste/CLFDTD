#include "vk_descriptors.h"

#include "vk_types.h"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);

    bindingFlags.push_back(0);
}

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkDescriptorBindingFlags flags) {
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = descriptorCount;
    newbind.descriptorType = type;

    bindings.push_back(newbind);

    bindingFlags.push_back(flags);
}

void DescriptorLayoutBuilder::clear() {
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages) {
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    bindingFlagsInfo.pNext = nullptr;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = &bindingFlagsInfo;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = 0;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
            });
    }

    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device) {
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device) {
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device) {
    VkDescriptorPool newPool;
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    }
    else {
        //need to create a new pool
        newPool = create_pool(device, setsPerPool, ratios);

        setsPerPool = static_cast<uint32_t>(setsPerPool * 1.5);
        if (setsPerPool > 4092) {
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio& ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * setCount)
            });
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
    return newPool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
    ratios.clear();

    for (auto r : poolRatios) {
        ratios.push_back(r);
    }

    VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

    setsPerPool = static_cast<uint32_t>(maxSets * 1.5); //grow it next allocation

    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device) {
    for (auto p : readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device) {
    for (auto p : readyPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    readyPools.clear();
    for (auto p : fullPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, uint32_t bindlessDescriptorBinding, uint32_t bindlessDescriptorCount) {
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    uint32_t counts[1]{ bindlessDescriptorBinding + bindlessDescriptorCount };

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
    variableAllocInfo.descriptorSetCount = 1;
    variableAllocInfo.pDescriptorCounts = counts;

    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO  };
    allocInfo.pNext = &variableAllocInfo;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    VkDescriptorBufferInfo info{
        .buffer = buffer,
        .offset = offset,
        .range = size
    };

    uint32_t writeInfoIndex = (uint32_t)bufferInfos.size();
    bufferInfos.push_back(info);
    writeInfoIndices.push_back(writeInfoIndex);
    writeInfoTypes.push_back(INFO_BUFFER);

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = nullptr;

    writes.push_back(write);
}

void DescriptorWriter::write_tlas(int binding, VkAccelerationStructureKHR* pAccelerationStructure)
{
    VkWriteDescriptorSetAccelerationStructureKHR writeAS{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    writeAS.pAccelerationStructures = pAccelerationStructure;
    writeAS.accelerationStructureCount = 1;

    uint32_t writeInfoIndex = (uint32_t)accelerationStructureInfos.size();
    accelerationStructureInfos.push_back(writeAS);
    writeInfoIndices.push_back(writeInfoIndex);
    writeInfoTypes.push_back(INFO_TLAS);
    
    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write.pNext = nullptr;

    writes.push_back(write);
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)  {
    VkDescriptorImageInfo info{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout
    };

    uint32_t writeInfoIndex = (uint32_t)imageInfos.size();
    imageInfos.push_back(info);
    writeInfoIndices.push_back(writeInfoIndex);
    writeInfoTypes.push_back(INFO_IMAGE);

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = nullptr;

    writes.push_back(write);
}

void DescriptorWriter::write_images(int binding, uint32_t descriptorCount, VkImageView* images, VkImageLayout* layouts, VkSampler sampler, VkDescriptorType type)
{
    for (uint32_t i = 0; i < descriptorCount; i++) {
        VkDescriptorImageInfo& info = unboundedArrayImageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = images[i],
            .imageLayout = layouts[i]
        });
    }

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = descriptorCount;
    write.descriptorType = type;
    write.pImageInfo = unboundedArrayImageInfos.data();

    writes.push_back(write);

    writeInfoIndices.push_back(0);
    writeInfoTypes.push_back(INFO_IMAGES_UNBOUNDED);
}

void DescriptorWriter::clear() {
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();
    unboundedArrayImageInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {
    uint32_t i = 0;
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;

        uint32_t writeInfoIndex = writeInfoIndices[i];
        InfoType infoType = writeInfoTypes[i];

        switch (infoType) {
        case INFO_BUFFER:
        {
            write.pBufferInfo = &bufferInfos[writeInfoIndex];
            break;
        }
        case INFO_IMAGE:
        {
            write.pImageInfo = &imageInfos[writeInfoIndex];
            break;
        }
        case INFO_TLAS:
        {
            write.pNext = &accelerationStructureInfos[writeInfoIndex];
            break;
        }
        case INFO_IMAGES_UNBOUNDED:
        {
            break;
        }
        default:
        {
            assert(false);
            break;
        }
        }

        i++;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}