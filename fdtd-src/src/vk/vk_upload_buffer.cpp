#include "vk_upload_buffer.h"

#include "vk_engine.h"
#include "vk_synchronisation.h"

void UploadBuffer::reset()
{
    contentSize = 0;
}

void UploadBuffer::resize(uint32_t newSize, VulkanEngine* engine)
{
    if (newSize == 0 || newSize <= bufferSize) {
        return;
    }

    if (bufferSize > 0) {
        vmaUnmapMemory(engine->_allocator, hostAllocation);
        vmaDestroyBuffer(engine->_allocator, hostBuffer, hostAllocation);
        vmaDestroyBuffer(engine->_allocator, gpuBuffer, gpuAllocation);
    }

    bufferSize = newSize;

    VkBufferCreateInfo hostBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    hostBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    hostBufferCreateInfo.size = bufferSize;

    VmaAllocationCreateInfo hostAllocInfo = {};
    hostAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    hostAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    hostAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vmaCreateBufferWithAlignment(engine->_allocator, &hostBufferCreateInfo, &hostAllocInfo, BUFFER_ALIGNMENT, &hostBuffer, &hostAllocation, &hostInfo);
    vmaMapMemory(engine->_allocator, hostAllocation, &hostData);

    VkBufferCreateInfo gpuBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    gpuBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    gpuBufferCreateInfo.size = bufferSize;

    VmaAllocationCreateInfo gpuAllocInfo = {};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateBufferWithAlignment(engine->_allocator, &gpuBufferCreateInfo, &gpuAllocInfo, BUFFER_ALIGNMENT, &gpuBuffer, &gpuAllocation, &gpuInfo);

    VkBufferDeviceAddressInfo deviceAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    deviceAddressInfo.buffer = gpuBuffer;
    address = vkGetBufferDeviceAddress(engine->_device, &deviceAddressInfo);
}

void* UploadBuffer::append(uint32_t appended)
{
    uint32_t charWriteIndex = contentSize;
    contentSize += appended;
    assert(contentSize <= bufferSize);
    return (char*)hostData + charWriteIndex;
}

void UploadBuffer::upload(VkCommandBuffer cmd, VulkanEngine* engine) const
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

void UploadBuffer::deallocate(VulkanEngine* engine)
{
    if (hostAllocation) {
        vmaUnmapMemory(engine->_allocator, hostAllocation);
        vmaDestroyBuffer(engine->_allocator, hostBuffer, hostAllocation);
    }
    if (gpuAllocation) {
        vmaDestroyBuffer(engine->_allocator, gpuBuffer, gpuAllocation);
    }

    bufferSize = 0;
    address = 0;
    reset();
}
