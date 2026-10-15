#include "engine.hpp"
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


    cubeMesh.vertexBuffer = Buffer::create(
        allocator,
        sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    cubeMesh.vertexBuffer.upload_data<Vertex>(allocator, vertices);

    cubeMesh.indexBuffer = Buffer::create(
        allocator,
        sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    cubeMesh.indexBuffer.upload_data<uint32_t>(allocator, indices);

    cubeMesh.indexCount = sizeof(indices) / sizeof(indices[0]);

    generate_cone_mesh();

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

void VulkanEngine::generate_cone_mesh()
{
    std::vector<Vertex> coneVerts;
    std::vector<uint32_t> coneIndices;

    coneVerts.push_back({glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f)}); 

    const int segments { 16 };
    for (int i = 0; i < segments; ++i)
    {
        float angle { static_cast<float>(i) / segments * 2.0f * glm::pi<float>() };
        coneVerts.push_back({
            glm::vec3(std::cos(angle), std::sin(angle), -1.0f), 
            glm::vec3(0.0f)
        });
    }

    for (int i = 1; i <= segments; ++i)
    {
        int next { (i % segments) + 1 };
        coneIndices.push_back(0);
        coneIndices.push_back(i);
        coneIndices.push_back(i);
        coneIndices.push_back(next);
    }

    coneMesh.vertexBuffer = Buffer::create(internal::context.allocator, coneVerts.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
    coneMesh.vertexBuffer.upload_data<Vertex>(internal::context.allocator, coneVerts);

    coneMesh.indexBuffer = Buffer::create(internal::context.allocator, coneIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
    coneMesh.indexBuffer.upload_data<uint32_t>(internal::context.allocator, std::span{coneIndices});

    coneMesh.indexCount = coneIndices.size();
}



void VulkanEngine::init()
{
    descriptorAllocator.init(graphics::internal::context.device);
    init_buffers();
    init_descriptors();

    Shader litVertShader(std::filesystem::current_path() / "shaders" / "lit_shader.vert.spv");
    Shader litFragShader(std::filesystem::current_path() / "shaders" / "lit_shader.frag.spv");
    if (!litVertShader.is_load() || !litFragShader.is_load())
    {
        std::println(stderr, "ERROR: Failed to load shaders! Check your file paths.");
        return;
    }

    Shader unlitVertShader(std::filesystem::current_path() / "shaders" / "unlit_shader.vert.spv");
    Shader unlitFragShader(std::filesystem::current_path() / "shaders" / "unlit_shader.frag.spv");
    if (!unlitVertShader.is_load() || !unlitFragShader.is_load())
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
    builder._pipelineLayout = pipelineLayout;
    builder._vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = sizeof(bindingDesc) / sizeof(bindingDesc[0]),
        .pVertexBindingDescriptions = bindingDesc,
        .vertexAttributeDescriptionCount = sizeof(attrDesc) / sizeof(attrDesc[0]),
        .pVertexAttributeDescriptions = attrDesc
    };
    builder.set_shaders(litVertShader, litFragShader);
    litPipeline = builder.build_pipeline(internal::context.device);

    builder.set_shaders(unlitVertShader, unlitFragShader);
    unlitPipeline = builder.build_pipeline(internal::context.device);

    builder.set_shaders(unlitVertShader, unlitFragShader)
           .set_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
           .set_polygon_mode(VK_POLYGON_MODE_FILL); 
    linePipeline = builder.build_pipeline(internal::context.device);
    
}

void VulkanEngine::cleanup()
{
    VkDevice device = graphics::internal::context.device;

    if (litPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, litPipeline, nullptr);
    }
    if (unlitPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, unlitPipeline, nullptr);
    }
    if (linePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, linePipeline, nullptr);
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


    if (litPipeline == VK_NULL_HANDLE || unlitPipeline == VK_NULL_HANDLE)
    {
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
        return;
    }

    VkViewport viewport
    {
        0.0f, 0.0f, 
        (float)ctx.swapchain_extent.width, 
        (float)ctx.swapchain_extent.height, 
        0.0f, 1.0f
    };
    VkRect2D scissor
    {
        {0, 0},
        ctx.swapchain_extent
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);


    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(ctx.allocator, dynamicObjectBuffer.allocation, &allocInfo);
    char* pMappedData { static_cast<char*>(allocInfo.pMappedData) };

    size_t current_dynamic_index { 0uz };

    auto draw_batch = [&](VkPipeline pipeline, std::span<const RenderObjectData> render_objects)
    {
        if (render_objects.empty()) { return; }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        const Mesh* lastMesh = nullptr;

        for (const auto& obj : render_objects)
        {
            if (current_dynamic_index >= maxObjects) { break; }


            if (lastMesh != obj.mesh)
            {
                VkDeviceSize offset { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->vertexBuffer.buffer, &offset);
                vkCmdBindIndexBuffer(cmd, obj.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                lastMesh = obj.mesh;
            }

            ObjectUniform objUbo 
            {
                obj.modelMatrix,
                obj.normalMatrix,
                obj.material
            };
            std::memcpy(pMappedData + (current_dynamic_index * dynamicAlignment), &objUbo, sizeof(ObjectUniform));

            uint32_t dynamicOffset { static_cast<uint32_t>(current_dynamic_index * dynamicAlignment) };

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
            
            vkCmdDrawIndexed(cmd, obj.mesh->indexCount, 1, 0, 0, 0);

            ++current_dynamic_index;
        }
    };

    draw_batch(litPipeline, data.litObjects);
    draw_batch(unlitPipeline, data.unlitObjects);
    draw_batch(linePipeline, data.lineObjects);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core