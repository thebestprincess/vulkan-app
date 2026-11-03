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
    PipelineBuilder& set_topology(VkPrimitiveTopology topology);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& disable_depth_test();
    
    VkPipeline build_pipeline(VkDevice device);
private:
    void clear();
};