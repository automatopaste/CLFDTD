#include "vk_dynamic_buffer.h"

#include "vk_engine.h"
#include "vk_synchronisation.h"

constexpr VkBufferUsageFlags DYNAMIC_BUFFER_HOST_USAGE = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
constexpr VkBufferUsageFlags DYNAMIC_BUFFER_DEVICE_USAGE = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
constexpr uint32_t MIN_ALLOC_SIZE = 1024;

void DynamicUploadBuffer::reset()
{
    contentSize = 0;
}

void* DynamicUploadBuffer::fit(uint32_t size, VulkanEngine* engine)
{
    if (size > hostBufferSize) {
        uint32_t newSize = std::max(MIN_ALLOC_SIZE, hostBufferSize * 2);
        while (newSize < size) {
            newSize *= 2;
        }
        reallocate(newSize, engine);
    }

    return hostData;
}

void* DynamicUploadBuffer::append(uint32_t appended, VulkanEngine* engine)
{
    uint32_t charWriteIndex = contentSize;
    contentSize += appended;

    if (contentSize > hostBufferSize) {
        uint32_t size = std::max(MIN_ALLOC_SIZE, hostBufferSize * 2);
        while (size < contentSize) {
            size *= 2;
        }
        reallocate(size, engine);
    }

    return (char*)hostData + charWriteIndex;
}

void DynamicUploadBuffer::flush(VulkanEngine* engine) const
{
    if (hostBufferSize == 0) {
        return;
    }

    VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    range.memory = hostInfo.deviceMemory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;

    vkFlushMappedMemoryRanges(engine->_device, 1, &range);
}

void DynamicUploadBuffer::upload(VkCommandBuffer cmd, VulkanEngine* engine) const
{
    if (contentSize == 0) {
        return;
    }

    VkBufferCopy2 copy2{ VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copy2.dstOffset = 0;
    copy2.srcOffset = 0;
    copy2.size = contentSize;

    VkCopyBufferInfo2 copyInfo2{ VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo2.dstBuffer = gpuBuffer;
    copyInfo2.srcBuffer = hostBuffer;
    copyInfo2.pRegions = &copy2;
    copyInfo2.regionCount = 1;

    vkCmdCopyBuffer2(cmd, &copyInfo2);
}

void DynamicUploadBuffer::deallocate(VulkanEngine* engine)
{
    if (hostAllocation) {
        vmaUnmapMemory(engine->_allocator, hostAllocation);
        vmaDestroyBuffer(engine->_allocator, hostBuffer, hostAllocation);
    }
    if (gpuAllocation) {
        vmaDestroyBuffer(engine->_allocator, gpuBuffer, gpuAllocation);
    }

    hostBufferSize = 0;
    gpuBufferSize = 0;
    address = 0;
    reset();
}

void DynamicUploadBuffer::reallocate(uint32_t size, VulkanEngine* engine)
{
    bool moveHostBuffer = hostBuffer;
    bool destroyGPUBuffer = gpuBuffer;

    uint32_t oldHostBufferSize = hostBufferSize;
    VkBuffer oldHostBuffer = hostBuffer;
    VmaAllocation oldHostAllocation = hostAllocation;
    void* oldHostData = hostData;

    VkBufferCreateInfo hostBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    hostBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    hostBufferCreateInfo.size = size;

    VmaAllocationCreateInfo hostAllocInfo = {};
    hostAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    hostAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    hostAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vmaCreateBufferWithAlignment(engine->_allocator, &hostBufferCreateInfo, &hostAllocInfo, BUFFER_ALIGNMENT, &hostBuffer, &hostAllocation, &hostInfo);
    vmaMapMemory(engine->_allocator, hostAllocation, &hostData);

    hostBufferSize = size;

    if (moveHostBuffer) {
        memcpy(hostData, oldHostData, oldHostBufferSize);

        vmaUnmapMemory(engine->_allocator, oldHostAllocation);
        vmaDestroyBuffer(engine->_allocator, oldHostBuffer, oldHostAllocation);
    }
    if (destroyGPUBuffer) {
        vmaDestroyBuffer(engine->_allocator, gpuBuffer, gpuAllocation);
    }

    VkBufferCreateInfo gpuBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    gpuBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    gpuBufferCreateInfo.size = size;

    gpuBufferSize = size;

    VmaAllocationCreateInfo gpuAllocInfo = {};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateBufferWithAlignment(engine->_allocator, &gpuBufferCreateInfo, &gpuAllocInfo, BUFFER_ALIGNMENT, &gpuBuffer, &gpuAllocation, &gpuInfo);

    VkBufferDeviceAddressInfo deviceAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    deviceAddressInfo.buffer = gpuBuffer;
    address = vkGetBufferDeviceAddress(engine->_device, &deviceAddressInfo);
}

void DynamicBuffer::fit(uint32_t size, VulkanEngine* engine)
{
    if (capacity >= size) {
        return;
    }

    if (buffer) {
        destroy(engine);
    }
    
    // round size up to nearest power of 2
    uint32_t v = size;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    alloc(v, engine, usage);
}

void DynamicBuffer::alloc(uint32_t size, VulkanEngine* engine, VkBufferUsageFlags usage)
{
    this->usage = usage;

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = (VkDeviceSize)size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaallocInfo.flags = 0;

    VK_CHECK(vmaCreateBufferWithAlignment(engine->_allocator, &bufferInfo, &vmaallocInfo, BUFFER_ALIGNMENT, &buffer, &allocation, &info));

    VkBufferDeviceAddressInfo deviceAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    deviceAddressInfo.buffer = buffer;
    address = vkGetBufferDeviceAddress(engine->_device, &deviceAddressInfo);

    capacity = size;
}

void DynamicBuffer::destroy(VulkanEngine* engine) {
    assert(buffer);
    vmaDestroyBuffer(engine->_allocator, buffer, allocation);
    capacity = 0;
    address = 0;
    buffer = 0;
}