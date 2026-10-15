#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

class PipelineBuilder final
{
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;

	PipelineBuilder();

    PipelineBuilder& set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& set_topology(VkPrimitiveTopology topology);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& disable_depth_test();
    
    VkPipeline build_pipeline(VkDevice device);
private:
    void clear();
};