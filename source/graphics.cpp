#include "graphics.hpp"
#include "gpu_resources.hpp"
#include "graphics_internal.hpp"
#include "common.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstring>
#include <print>


namespace graphics::core
{



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

    vertexBuffer = Buffer::create(
        allocator,
        sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    vertexBuffer.upload_data<Vertex>(allocator, vertices);

    indexBuffer = Buffer::create(
        allocator,
        sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    indexBuffer.upload_data<uint32_t>(allocator, indices);

    sceneUniformBuffer = Buffer::create(
        allocator,
        sizeof(SceneUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    ); 

    objectUniformBuffer = Buffer::create(
        allocator,
        sizeof(ObjectUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    object2UniformBuffer = Buffer::create(
        allocator,
        sizeof(ObjectUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );

    pointLightBuffer = Buffer::create(
        allocator, 
        sizeof(PointLight) * maxPointLights, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_AUTO
    );
    spotLightBuffer = Buffer::create(
        allocator, 
        sizeof(SpotLight) * maxSpotLights, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_AUTO
    );
}

void VulkanEngine::init_descriptors()
{
    VkDevice device = graphics::internal::context.device;
    DescriptorBuilder builder;

    VkDescriptorBufferInfo sceneInfo { sceneUniformBuffer, 0, sizeof(SceneUniform) };
    VkDescriptorBufferInfo pointLightInfo { pointLightBuffer, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo spotLightInfo { spotLightBuffer, 0, VK_WHOLE_SIZE };
    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sceneInfo)
           .bind_buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, pointLightInfo)
           .bind_buffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, spotLightInfo)
           .build(device, descriptorAllocator, sceneSetLayout, sceneDescriptorSet);

    VkDescriptorBufferInfo objInfo{objectUniformBuffer, 0, sizeof(ObjectUniform)};
    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, objInfo)
           .build(device, descriptorAllocator, objectSetLayout, objectDescriptorSet);

    VkDescriptorBufferInfo obj2Info{object2UniformBuffer, 0, sizeof(ObjectUniform)};
    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, obj2Info)
           .build_existing_layout(device, descriptorAllocator, objectSetLayout, object2DescriptorSet);
}

void VulkanEngine::init()
{
    descriptorAllocator.init(graphics::internal::context.device);
    init_buffers();
    init_descriptors();

    Shader vertShader(std::filesystem::current_path() / "shaders" / "shader.vert.spv");
    Shader fragShader(std::filesystem::current_path() / "shaders" / "shader.frag.spv");

    if (!vertShader.is_load() || !fragShader.is_load())
    {
        std::println(stderr, "ERROR: Failed to load shaders! Check your file paths.");
        return;
    }

    VkDescriptorSetLayout layouts[] { sceneSetLayout, objectSetLayout };
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
    VkVertexInputAttributeDescription attrDesc[2]
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position)
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal)
        }
    };

    PipelineBuilder builder;
    builder.set_shaders(vertShader, fragShader);
    builder._pipelineLayout = pipelineLayout;

    builder._vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrDesc
    };
    builder._inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    pipeline = builder.build_pipeline(internal::context.device);
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

    descriptorAllocator.cleanup();

    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
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


    SceneUniform sceneUbo { std::move(data.sceneUniform) };
    sceneUniformBuffer.upload_data<SceneUniform>(ctx.allocator, std::span{&sceneUbo, 1});

    if (!data.pointLights.empty())
    {
        pointLightBuffer.upload_data<PointLight>(ctx.allocator, data.pointLights);
    }
    if (!data.spotLights.empty())
    {
        spotLightBuffer.upload_data<SpotLight>(ctx.allocator, data.spotLights);
    }


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

            ObjectUniform objUbo{ data.objects[i].modelMatrix, { data.objects[i].material } };
            
            VmaAllocationInfo objAllocInfo;
            vmaGetAllocationInfo(ctx.allocator, objectAllocations[i], &objAllocInfo);
            std::memcpy(objAllocInfo.pMappedData, &objUbo, sizeof(objUbo));

            VkDescriptorSet sets[] = { sceneDescriptorSet, objectSets[i] };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);
            
            vkCmdDrawIndexed(cmd, sizeof(indices) / sizeof(indices[0]), 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core