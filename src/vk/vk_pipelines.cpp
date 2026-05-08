#include "vk_pipelines.h"

#include <fstream>
#include "vk_initialisers.h"
#include <format>
#include <vulkan/vk_enum_string_helper.h>

#include <filesystem>
#include <iostream>

#include "vk_engine.h"
#include "vk_pipelines.h"

constexpr const char* SHADERS_DIR = "shaders/";
constexpr const char* SPIRV_OUTPUT = "shaders/bin/";

void vkutil::compile_shader_module(const char* filePath, shaderc::Compiler& compiler, shaderc::CompileOptions& options, VkDevice device) {
    fmt::print("{}", std::string("compiling " + std::string(filePath) + "...\n"));

    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path p = cwd / SHADERS_DIR / filePath;

    std::vector<char> buffer;

    std::string pc = p.string();

    std::ifstream file(pc.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cout << "failed to open shader file " << filePath << std::endl;
        return;
    }

    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);
    buffer.resize(fileSize);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    shaderc_shader_kind kind{};
    if (p.extension().string() == ".comp") {
        kind = shaderc_glsl_compute_shader;
    }
    else if (p.extension().string() == ".vert") {
        kind = shaderc_glsl_vertex_shader;
    }
    else if (p.extension().string() == ".frag") {
        kind = shaderc_glsl_fragment_shader;
    }
    else if (p.extension().string() == ".rgen") {
        kind = shaderc_glsl_raygen_shader;
    }
    else if (p.extension().string() == ".rchit") {
        kind = shaderc_glsl_closesthit_shader;
    }
    else if (p.extension().string() == ".rmiss") {
        kind = shaderc_glsl_miss_shader;
    }
    else {
        kind = shaderc_glsl_infer_from_source;
    }

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(buffer.data(), fileSize, kind, filePath, "main", options);

    size_t numErrors = result.GetNumErrors();
    if (numErrors > 0) {
        std::string errmsg = result.GetErrorMessage();
        std::string errstr = numErrors +  " shader compilation errors:\n";
        errstr.append(errmsg);
        fmt::print("{}", errstr);
    }
    else {
        fmt::print("{}", std::string("compile success " + std::string(filePath) + "\n"));
    }

    /*size_t dataSize = shaderc_result_get_length(result);

    const char* data = shaderc_result_get_bytes(result);
    const uint32_t* pCode = reinterpret_cast<const uint32_t*>(data);*/

    std::string name = p.filename().string();
    std::string outFileName = name + ".spirv";
    std::string outFilePath = SPIRV_OUTPUT + outFileName;

    std::ofstream output(outFilePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    output.seekp(0);
    for (const uint32_t* i = result.begin(); i < result.end(); i++) {
        output.write((const char*)i, sizeof(uint32_t));
    }
    //output.write(data, dataSize);
    output.close();
}

void vkutil::load_shader_module(const char* fileName, VkDevice device, VkShaderModule* outShaderModule) {
    std::filesystem::path f(fileName);
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path p = cwd / SPIRV_OUTPUT / f.filename();

    std::string str = p.string() + ".spirv";

    // open the file. With cursor at the end
    std::ifstream file(str.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        fmt::print("{}", std::string("failed to open file " + str + "\n"));
        return;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        fmt::print("{}", std::string("failed to load shader " + std::string(fileName) + "\n"));
    }
    else {
        fmt::print("{}", std::string("loaded shader " + std::string(fileName) + "\n"));
    }
    *outShaderModule = shaderModule;
}

bool vkutil::check_file_exists(const char* fileName) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path p = cwd / SPIRV_OUTPUT / fileName;

    std::string str = p.string() + ".spirv";

    if (!std::filesystem::exists(str)) {
        return false;
    }

    auto size = std::filesystem::file_size(str);
    return size > 0;
}

void vkutil::compile_and_load(const char* fileName, VulkanEngine* engine, VkShaderModule* outShaderModule)
{
    compile_shader_module(fileName, engine->_compiler, engine->_compileOptions, engine->_device);
    load_shader_module(fileName, engine->_device, outShaderModule);
}

void vkutil::check_compile_load(const char* fileName, VulkanEngine* engine, VkShaderModule* outShaderModule, bool compile)
{
    if (compile || !check_file_exists(fileName)) {
        compile_shader_module(fileName, engine->_compiler, engine->_compileOptions, engine->_device);
    }
    load_shader_module(fileName, engine->_device, outShaderModule);
}

void PipelineBuilder::clear() {
    //clear all of the structs we need back to 0 with their correct stype	
    _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

    _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    _pipelineLayout = {};

    _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    _shaderStages.clear();

    _colorAttachmentFormats.clear();
    _colorBlendAttachments.clear();
}

void PipelineBuilder::build(VkDevice device, VkPipeline& dest) {
    _renderInfo.colorAttachmentCount = static_cast<uint32_t>(_colorAttachmentFormats.size());
    _renderInfo.pColorAttachmentFormats = _colorAttachmentFormats.size() > 0 ? _colorAttachmentFormats.data() : nullptr;

    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(_colorBlendAttachments.size());
    colorBlending.pAttachments = _colorBlendAttachments.size() > 0 ? _colorBlendAttachments.data() : nullptr;


    //completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    //connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &_renderInfo;

    pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);

    if (result != VK_SUCCESS) {
        const char* errStr = string_VkResult(result);
        fmt::print("failed to create pipeline: {}\n", errStr);
        newPipeline = VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    
    dest = std::move(newPipeline);
}

void PipelineBuilder::set_vertex_shader_only(VkShaderModule vertex)
{
    _shaderStages.clear();
    _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex));
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader) {
    _shaderStages.clear();

    _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

    _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::push_stage(VkShaderModule shaderModule, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    shaderStageCreateInfo.stage = stage;
    shaderStageCreateInfo.module = shaderModule;
    shaderStageCreateInfo.pName = "main";
    _shaderStages.push_back(shaderStageCreateInfo);
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology) {
    _inputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    _inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
    _rasterizer.polygonMode = mode;
    _rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
    _rasterizer.cullMode = cullMode;
    _rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none() {
    _multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    _multisampling.minSampleShading = 1.0f;
    _multisampling.pSampleMask = nullptr;
    //no alpha to coverage either
    _multisampling.alphaToCoverageEnable = VK_FALSE;
    _multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::add_blend_disabled() {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    //default write mask
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    //no blending
    colorBlendAttachment.blendEnable = VK_FALSE;
    _colorBlendAttachments.push_back(colorBlendAttachment);
}

void PipelineBuilder::add_color_attachment_format(VkFormat format) {
    _colorAttachmentFormats.push_back(format);
}

void PipelineBuilder::clear_color_attachments()
{
    _colorAttachmentFormats.clear();
}

void PipelineBuilder::set_depth_format(VkFormat format) {
    _renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depth_attachment()
{
    _renderInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
}

void PipelineBuilder::disable_depthtest() {
    _depthStencil.depthTestEnable = VK_FALSE;
    _depthStencil.depthWriteEnable = VK_FALSE;
    _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.stencilTestEnable = VK_FALSE;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.f;
    _depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
    _depthStencil.depthTestEnable = VK_TRUE;
    _depthStencil.depthWriteEnable = depthWriteEnable;
    _depthStencil.depthCompareOp = op;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.stencilTestEnable = VK_FALSE;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.f;
    _depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::add_blend_additive() {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    _colorBlendAttachments.push_back(colorBlendAttachment);
}

void PipelineBuilder::add_blend_alphablend() {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    _colorBlendAttachments.push_back(colorBlendAttachment);
}

void PipelineBuilder::clear_blend_attachments()
{
    _colorBlendAttachments.clear();
}

VkPipeline ComputePipelineBuilder::build_pipeline(VkDevice device)
{
    _computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    _computePipelineCreateInfo.pNext = nullptr;
    _computePipelineCreateInfo.layout = _pipelineLayout;

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &_computePipelineCreateInfo, nullptr, &pipeline));

    return pipeline;
}

void ComputePipelineBuilder::set_shader(VkShaderModule computeShader)
{
    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeShader;
    stageinfo.pName = "main";
    stageinfo.flags = createFlags;

    if (specInfo.pData) {
        stageinfo.pSpecializationInfo = &specInfo;
    }

    _computePipelineCreateInfo.stage = stageinfo;
}

void ComputePipelineBuilder::set_shader_flags(VkPipelineShaderStageCreateFlags flags)
{
    createFlags = flags;
}

void ComputePipelineBuilder::set_pipeline_layout(VkPipelineLayout layout)
{
    _pipelineLayout = layout;
}

void ComputePipelineBuilder::set_spec_constants(VkSpecializationInfo specializationInfo)
{
    specInfo = specializationInfo;
}

void ComputePipelineBuilder::build(VkDevice device, VkPipeline& dest)
{
    _computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    _computePipelineCreateInfo.pNext = nullptr;
    _computePipelineCreateInfo.layout = _pipelineLayout;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &_computePipelineCreateInfo, nullptr, &dest));
}

void LayoutBuilder::set_layout(VkDescriptorSetLayout layout)
{
    _layouts.push_back(layout);
}

void LayoutBuilder::push_constant(size_t size, VkShaderStageFlags stages)
{
    _pushConstant.offset = 0;
    _pushConstant.size = static_cast<uint32_t>(size);
    _pushConstant.stageFlags = stages;
}

void LayoutBuilder::build(VkDevice device, VkPipelineLayout& dest)
{
    assert(_pushConstant.size != 0);

    info.pSetLayouts = _layouts.data();
    info.setLayoutCount = static_cast<uint32_t>(_layouts.size());
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &_pushConstant;

    VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &dest));
}
