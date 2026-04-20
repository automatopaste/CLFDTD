#pragma once

#include <vector>
#include <span>
#include <deque>

#include <Volk/volk.h>

struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void add_binding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkDescriptorBindingFlags flags);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages);

};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, uint32_t bindlessDescriptorBinding, uint32_t bindlessDescriptorCount);

private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};

struct DescriptorWriter {

    enum InfoType {
        INFO_UNDEFINED,
        INFO_BUFFER,
        INFO_IMAGE,
        INFO_IMAGES_UNBOUNDED,
        INFO_TLAS
    };

    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> unboundedArrayImageInfos;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructureInfos;
    std::vector<uint32_t> writeInfoIndices;
    std::vector<InfoType> writeInfoTypes;

    void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void write_images(int binding, uint32_t descriptorCount, VkImageView* images, VkImageLayout* layouts, VkSampler sampler, VkDescriptorType type);
    void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
    void write_tlas(int binding, VkAccelerationStructureKHR* pAccelerationStructure);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};  