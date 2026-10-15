#include "pipeline_builder.hpp"
#include "graphics_internal.hpp"

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

PipelineBuilder& PipelineBuilder::disable_depth_test()
{
    _depthStencil.depthTestEnable = false;
    _depthStencil.depthWriteEnable = VK_FALSE;
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

    VkPipelineColorBlendStateCreateInfo colorBlending
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .attachmentCount = 1,
        .pAttachments = &_colorBlendAttachment,
    };

    VkDynamicState dynamic_states[]
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    
    VkPipelineDynamicStateCreateInfo dynamicInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
        .pDynamicStates = dynamic_states,
    };
    
    VkGraphicsPipelineCreateInfo pipelineInfo 
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
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
        .renderPass = graphics::internal::context.render_pass,
        .subpass = 0, 
    };
    
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &newPipeline) != VK_SUCCESS)
    {
        std::println(stderr, "failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    else
    {
        return newPipeline;
    }

}

void PipelineBuilder::clear()
{
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
}
