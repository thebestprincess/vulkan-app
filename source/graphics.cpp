#include "graphics.hpp"
#include "gpu_resources.hpp"
#include "graphics_internal.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <print>


namespace graphics::core
{

namespace utils
{

VkShaderModule load_shader_module(
    std::filesystem::path& path
) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const std::streampos pos { file.tellg() };
    if (pos == std::streampos(-1)) return VK_NULL_HANDLE;

    const size_t size { static_cast<size_t>(pos) };
    std::vector<uint32_t> buffer(size / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    VkShaderModuleCreateInfo info
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = buffer.data(),
    };

    VkShaderModule result;
    if (vkCreateShaderModule(graphics::internal::context.device,
                             &info, nullptr, &result) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    } 

    return result;
}

} // namespace graphics::core::utils


void PipelineBuilder::setup()
{
    _inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 
    };

    _rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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

    _pipelineLayout = {};

    _depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    _shaderStages.clear();
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

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    _shaderStages.clear();

    _shaderStages.push_back(
        VkPipelineShaderStageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = std::move(vertexShader),
            .pName = "main",
        }
    );

    _shaderStages.push_back(
        VkPipelineShaderStageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = std::move(fragmentShader), 
            .pName = "main",
        }
    );
}

void VulkanEngine::init_buffers()
{
    const auto& allocator = internal::context.allocator;    

    vertexBuffer = Buffer::create(allocator, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
    vertexBuffer.upload_data<Vertex>(allocator, vertices);

    indexBuffer = Buffer::create(allocator, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
    indexBuffer.upload_data<uint32_t>(allocator, indices);

    sceneUniformBuffer = Buffer::create(allocator, sizeof(SceneUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO); 

    objectUniformBuffer = Buffer::create(allocator, sizeof(ObjectUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
    object2UniformBuffer = Buffer::create(allocator, sizeof(ObjectUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
}

void VulkanEngine::init_descriptors()
{
    VkDevice& device = graphics::internal::context.device;

    VkDescriptorSetLayoutBinding cameraBinding
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    VkDescriptorSetLayoutCreateInfo cameraLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &cameraBinding
    };
    vkCreateDescriptorSetLayout(device, &cameraLayoutInfo, nullptr, &cameraSetLayout);


    VkDescriptorSetLayoutBinding objectBinding
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    VkDescriptorSetLayoutCreateInfo objectLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &objectBinding
    };
    vkCreateDescriptorSetLayout(device, &objectLayoutInfo, nullptr, &objectSetLayout);


    VkDescriptorPoolSize poolSize
    {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 10
    };
    VkDescriptorPoolCreateInfo poolInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 10,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetLayout layouts[] { cameraSetLayout, objectSetLayout, objectSetLayout };
    VkDescriptorSetAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 3,
        .pSetLayouts = layouts
    };
    VkDescriptorSet sets[3];
    vkAllocateDescriptorSets(device, &allocInfo, sets);
    cameraDescriptorSet = sets[0];
    objectDescriptorSet = sets[1];
    object2DescriptorSet = sets[2];

    VkDescriptorBufferInfo camInfo
    {
        .buffer = sceneUniformBuffer.buffer,
        .offset = 0,
        .range = sizeof(SceneUniform)
    };
    VkDescriptorBufferInfo objInfo
    {
        .buffer = objectUniformBuffer.buffer,
        .offset = 0,
        .range = sizeof(ObjectUniform)
    };
    VkDescriptorBufferInfo obj2Info
    {
        .buffer = object2UniformBuffer.buffer,
        .offset = 0,
        .range = sizeof(ObjectUniform)
    };

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = cameraDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &camInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = objectDescriptorSet;
    writes[1].dstBinding = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &objInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = object2DescriptorSet;
    writes[2].dstBinding = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &obj2Info;

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

void VulkanEngine::init()
{
    init_buffers();
    init_descriptors();

    std::filesystem::path vertPath = std::filesystem::current_path() / "shaders" / "shader.vert.spv";
    std::filesystem::path fragPath = std::filesystem::current_path() / "shaders" / "shader.frag.spv";
    VkShaderModule vertShader = utils::load_shader_module(vertPath);
    VkShaderModule fragShader = utils::load_shader_module(fragPath);

    if (vertShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
    {
        std::println(stderr, "ERROR: Failed to load shaders! Check your file paths.");
        return;
    }

    VkDescriptorSetLayout layouts[] { cameraSetLayout, objectSetLayout };
    VkPipelineLayoutCreateInfo layoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = layouts
    };
    vkCreatePipelineLayout(internal::context.device, &layoutInfo, nullptr, &pipelineLayout);

    VkVertexInputBindingDescription bindingDesc
    {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrDesc
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, position)
    };

    PipelineBuilder builder;
    builder.set_shaders(vertShader, fragShader);
    builder._pipelineLayout = pipelineLayout;

    builder._vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attrDesc
    };
    builder._inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    pipeline = builder.build_pipeline(internal::context.device);

    vkDestroyShaderModule(internal::context.device, vertShader, nullptr);
    vkDestroyShaderModule(internal::context.device, fragShader, nullptr);
}

void VulkanEngine::cleanup()
{
    VkDevice device = graphics::internal::context.device;

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, cameraSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
    }
}

void VulkanEngine::draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, const FrameRenderData& data)
{
    auto& ctx = graphics::internal::context;

    VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) { return; }


    SceneUniform sceneUbo{ data.view, data.proj };
    VmaAllocationInfo camAlloc;
    vmaGetAllocationInfo(ctx.allocator, sceneUniformBuffer.allocation, &camAlloc);
    std::memcpy(camAlloc.pMappedData, &sceneUbo, sizeof(sceneUbo));



    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}}; // Dark grey background layer
    clearValues[1].depthStencil = {1.0f, 0};           // Clear depth buffer

    VkRenderPassBeginInfo rpInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx.render_pass,
        .framebuffer = framebuffer,
        .renderArea = { .extent = ctx.swapchain_extent },
        .clearValueCount = 2,
        .pClearValues = clearValues
    };
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);


    if (pipeline != VK_NULL_HANDLE)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        

        VkViewport viewport {
            0.0f, 0.0f, 
            (float)ctx.swapchain_extent.width, 
            (float)ctx.swapchain_extent.height, 
            0.0f, 1.0f
        };
        VkRect2D scissor {{0, 0}, ctx.swapchain_extent};

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);


        VkDeviceSize offset { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);



        VkDescriptorSet objectSets[] = { objectDescriptorSet, object2DescriptorSet };
        VmaAllocation objectAllocations[] = { objectUniformBuffer.allocation, object2UniformBuffer.allocation };

        for (size_t i = 0; i < data.objects.size(); ++i)
        {
            if (i >= 2) break; 

            ObjectUniform objUbo{ data.objects[i].modelMatrix, data.objects[i].color };
            
            VmaAllocationInfo objAllocInfo;
            vmaGetAllocationInfo(ctx.allocator, objectAllocations[i], &objAllocInfo);
            std::memcpy(objAllocInfo.pMappedData, &objUbo, sizeof(objUbo));

            VkDescriptorSet sets[] = { cameraDescriptorSet, objectSets[i] };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);
            
            vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core