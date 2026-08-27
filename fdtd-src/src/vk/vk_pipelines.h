#pragma once

#include <vector>

#include <shaderc/shaderc.hpp>

#include "vk_types.h"

class VulkanEngine;

namespace vkutil {

    void compile_shader_module(const char* fileName, shaderc::Compiler& compiler, shaderc::CompileOptions& options, VkDevice device);

    void load_shader_module(const char* fileName, VkDevice device, VkShaderModule* outShaderModule);

    bool check_file_exists(const char* fileName);

    void compile_and_load(const char* fileName, VulkanEngine* engine, VkShaderModule* outShaderModule);

    void check_compile_load(const char* fileName, VulkanEngine* engine, VkShaderModule* outShaderModule, bool compile);

}

class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;

    std::vector<VkFormat> _colorAttachmentFormats;
    std::vector<VkPipelineColorBlendAttachmentState> _colorBlendAttachments;

    PipelineBuilder() { clear(); }

    void clear();

    void set_vertex_shader_only(VkShaderModule vertex);
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void push_stage(VkShaderModule shaderModule, VkShaderStageFlagBits stage);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void set_multisampling_none();
    void add_blend_disabled();
    void add_color_attachment_format(VkFormat format);
    void clear_color_attachments();
    void set_depth_format(VkFormat format);
    void disable_depth_attachment();
    void disable_depthtest();
    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
    void add_blend_additive();
    void add_blend_alphablend();
    void clear_blend_attachments();
    void build(VkDevice device, VkPipeline& dest);

};

class ComputePipelineBuilder {
public:
    VkPipelineShaderStageCreateInfo _computeShaderStage;
    VkPipelineLayout _pipelineLayout;
    VkComputePipelineCreateInfo _computePipelineCreateInfo{};
    VkSpecializationInfo specInfo{};
    VkPipelineShaderStageCreateFlags createFlags{ 0 };

    VkPipeline build_pipeline(VkDevice device);

    void set_shader(VkShaderModule computeShader);
    void set_shader_flags(VkPipelineShaderStageCreateFlags flags);
    void set_pipeline_layout(VkPipelineLayout layout);
    void set_spec_constants(VkSpecializationInfo specializationInfo);
    void build(VkDevice device, VkPipeline& dest);

};

class LayoutBuilder {
public:

    VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    std::vector<VkDescriptorSetLayout> _layouts;
    VkPushConstantRange _pushConstant{};

    void set_layout(VkDescriptorSetLayout layout);
    void push_constant(size_t size, VkShaderStageFlags stages);
    void build(VkDevice device, VkPipelineLayout& dest);

};