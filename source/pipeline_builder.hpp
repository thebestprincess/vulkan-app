#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

class PipelineBuilder final
{
public:
    PipelineBuilder();

public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineLayout _pipelineLayout;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;

    PipelineBuilder& set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& set_vertex_shader(VkShaderModule vertexShader);
    PipelineBuilder& set_topology(VkPrimitiveTopology topology);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& set_cull_mode(VkCullModeFlags cullMode);
    PipelineBuilder& disable_depth_test();
    PipelineBuilder& enable_depth_bias(bool dynamic = true, float constantFactor = 0.0f, float slopeFactor = 0.0f);
    PipelineBuilder& set_dynamic_rendering(VkFormat depthFormat, VkFormat colorFormat = VK_FORMAT_UNDEFINED);

    VkPipeline build_pipeline(VkDevice device);
    void clear();

private:
    bool _useDynamicRendering { false };
    VkFormat _colorAttachmentFormat { VK_FORMAT_UNDEFINED };
    VkFormat _depthAttachmentFormat { VK_FORMAT_UNDEFINED };
    std::vector<VkDynamicState> _dynamicStates;
};