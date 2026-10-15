#include "graphics.hpp"
#include "gpu_resources.hpp"
#include "graphics_internal.hpp"
#include "pipeline_builder.hpp"
#include "common.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstring>
#include <print>


namespace graphics::core
{

void VulkanEngine::init_buffers()
{
    const auto& allocator = internal::context.allocator;    
    const auto& ctx = internal::context;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &properties);
    size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;

    dynamicAlignment = sizeof(ObjectUniform);
    if (minUboAlignment > 0)
    {
        dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    dynamicObjectBuffer = Buffer::create(
        ctx.allocator,
        maxObjects * dynamicAlignment,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );

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

    VkDescriptorBufferInfo dynObjInfo { dynamicObjectBuffer, 0, sizeof(ObjectUniform) };
    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, dynObjInfo)
           .build(device, descriptorAllocator, objectSetLayout, dynamicObjectDescriptorSet);
}

void VulkanEngine::init()
{
    descriptorAllocator.init(graphics::internal::context.device);
    init_buffers();
    init_descriptors();

    Shader vertShader(std::filesystem::current_path() / "shaders" / "lit_shader.vert.spv");
    Shader fragShader(std::filesystem::current_path() / "shaders" / "lit_shader.frag.spv");
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

    VkVertexInputBindingDescription bindingDesc[]
    {
        {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
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
    builder.set_shaders(vertShader, fragShader).set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder._pipelineLayout = pipelineLayout;

    builder._vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = sizeof(bindingDesc) / sizeof(bindingDesc[0]),
        .pVertexBindingDescriptions = bindingDesc,
        .vertexAttributeDescriptionCount = sizeof(attrDesc) / sizeof(attrDesc[0]),
        .pVertexAttributeDescriptions = attrDesc
    };

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



        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(ctx.allocator, dynamicObjectBuffer.allocation, &allocInfo);
        char* pMappedData { static_cast<char*>(allocInfo.pMappedData) };

        size_t objectCount { std::min(data.objects.size(), maxObjects) };
        
        for (size_t i { 0 }; i < objectCount; ++i)
        {
            ObjectUniform objUbo { data.objects[i].modelMatrix, data.objects[i].normalMatrix, data.objects[i].material };
            std::memcpy(pMappedData + (i * dynamicAlignment), &objUbo, sizeof(ObjectUniform));
        }
        
        for (size_t i { 0 }; i < objectCount; ++i)
        {
            uint32_t dynamicOffset = static_cast<uint32_t>(i * dynamicAlignment);

            VkDescriptorSet sets[] { sceneDescriptorSet, dynamicObjectDescriptorSet };
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,                                  // first set
                sizeof(sets) / sizeof(sets[0]),
                sets,
                1,                                  // (dynamicOffsetCount) Have only one for dynamicObjectBuffer
                &dynamicOffset
            );

            vkCmdDrawIndexed(cmd, sizeof(indices) / sizeof(indices[0]), 1, 0, 0, 0);            
        }
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core