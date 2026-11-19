#include "pipeline_builder.hpp"
#include "graphics_internal.hpp"

#include <algorithm>
#include <print>

PipelineBuilder::PipelineBuilder()
{
    clear();
}

PipelineBuilder& PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    _shaderStages.clear();
    _shaderStages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShader,
        .pName = "main",
    });
    _shaderStages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentShader,
        .pName = "main",
    });
    return *this;
}

PipelineBuilder& PipelineBuilder::set_vertex_shader(VkShaderModule vertexShader)
{
    _shaderStages.clear();
    _shaderStages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShader,
        .pName = "main",
    });
    return *this;
}

PipelineBuilder& PipelineBuilder::set_topology(VkPrimitiveTopology topology)
{
    _inputAssembly.topology = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_polygon_mode(VkPolygonMode mode)
{
    _rasterizer.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode)
{
    _rasterizer.cullMode = cullMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::disable_depth_test()
{
    _depthStencil.depthTestEnable = VK_FALSE;
    _depthStencil.depthWriteEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::enable_depth_bias(bool dynamic, float constantFactor, float slopeFactor)
{
    _rasterizer.depthBiasEnable = VK_TRUE;
    if (dynamic)
    {
        if (std::ranges::find(_dynamicStates, VK_DYNAMIC_STATE_DEPTH_BIAS) == _dynamicStates.end())
        {
            _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        }
    }
    else
    {
        _rasterizer.depthBiasConstantFactor = constantFactor;
        _rasterizer.depthBiasSlopeFactor = slopeFactor;
    }
    return *this;
}

PipelineBuilder& PipelineBuilder::set_dynamic_rendering(VkFormat depthFormat, VkFormat colorFormat)
{
    _useDynamicRendering = true;
    _depthAttachmentFormat = depthFormat;
    _colorAttachmentFormat = colorFormat;
    return *this;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
    VkPipelineViewportStateCreateInfo viewportState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const bool hasColorAttachment = !_useDynamicRendering || (_colorAttachmentFormat != VK_FORMAT_UNDEFINED);

    VkPipelineColorBlendStateCreateInfo colorBlending
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .attachmentCount = hasColorAttachment ? 1u : 0u,
        .pAttachments = hasColorAttachment ? &_colorBlendAttachment : nullptr,
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size()),
        .pDynamicStates = _dynamicStates.data(),
    };

    VkPipelineRenderingCreateInfo renderingInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = (_colorAttachmentFormat != VK_FORMAT_UNDEFINED) ? 1u : 0u,
        .pColorAttachmentFormats = (_colorAttachmentFormat != VK_FORMAT_UNDEFINED) ? &_colorAttachmentFormat : nullptr,
        .depthAttachmentFormat = _depthAttachmentFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = _useDynamicRendering ? &renderingInfo : nullptr,
        .stageCount = static_cast<uint32_t>(_shaderStages.size()),
        .pStages = _shaderStages.data(),
        .pVertexInputState = &_vertexInputInfo,
        .pInputAssemblyState = &_inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &_rasterizer,
        .pMultisampleState = &_multisampling,
        .pDepthStencilState = &_depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicInfo,
        .layout = _pipelineLayout,
        .renderPass = _useDynamicRendering ? VK_NULL_HANDLE : graphics::internal::context.render_pass,
        .subpass = 0,
    };

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &newPipeline) != VK_SUCCESS)
    {
        std::println(stderr, "failed to create pipeline");
        return VK_NULL_HANDLE;
    }

    return newPipeline;
}

void PipelineBuilder::clear()
{
    _pipelineLayout = VK_NULL_HANDLE;
    _vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    _inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    _rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    _colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
    };

    _multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    _depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    _shaderStages.clear();
    _useDynamicRendering = false;
    _colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    _depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    _dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
}