#include "engine.hpp"
#include "gpu_resources.hpp"
#include "graphics_internal.hpp"
#include "pipeline_builder.hpp"
#include "common.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <print>

namespace graphics::core::geometry
{

void generate_cone_mesh(Mesh& mesh)
{
    std::vector<Vertex> coneVerts;
    std::vector<uint32_t> coneIndices;

    coneVerts.push_back(
        {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f),
            glm::vec2(0.0f)
        }
    ); 

    const int segments { 16 };
    for (int i = 0; i < segments; ++i)
    {
        float angle { static_cast<float>(i) / segments * 2.0f * glm::pi<float>() };
        coneVerts.push_back({
            glm::vec3(std::cos(angle), std::sin(angle), -1.0f), 
            glm::vec3(0.0f),
            glm::vec2(std::cos(angle)*0.5f + 0.5f, std::sin(angle)*0.5f + 0.5f)
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

    auto& alloc = internal::context.allocator;

    mesh.vertexBuffer = Buffer::create(
        alloc,
        coneVerts.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    mesh.vertexBuffer.upload_data<Vertex>(alloc, coneVerts);

    mesh.indexBuffer = Buffer::create(
        alloc,
        coneIndices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    mesh.indexBuffer.upload_data<uint32_t>(alloc, std::span{coneIndices});

    mesh.indexCount = coneIndices.size();
}

void generate_plane_mesh(Mesh& mesh)
{
    const std::array<Vertex, 4> planeVerts = {
        Vertex{{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
    };
    const std::array<uint32_t, 6> planeIndices = { 0, 2, 1, 2, 0, 3 };

    auto& alloc = internal::context.allocator;

    mesh.vertexBuffer = Buffer::create(
        alloc,
        sizeof(planeVerts),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    mesh.vertexBuffer.upload_data<Vertex>(alloc, planeVerts);

    mesh.indexBuffer = Buffer::create(
        alloc,
        sizeof(planeIndices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    mesh.indexBuffer.upload_data<uint32_t>(alloc, planeIndices);

    mesh.indexCount = planeIndices.size();
}

} // namespace graphics::geometry

namespace graphics::core
{
VulkanEngine::~VulkanEngine() { cleanup(); }

size_t VulkanEngine::load_texture(
    const std::filesystem::path& path,
    std::string name
) {
    Texture tex = upload_texture(path);

    VkDescriptorImageInfo imageInfo {
        .sampler = tex.sampler,
        .imageView = tex.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorSet texSet;
    DescriptorBuilder builder;
    builder.bind_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, imageInfo)
           .build_existing_layout(internal::context.device, descriptorAllocator, textureSetLayout, texSet);

    loadedTextures.push_back({ std::move(name), std::move(tex), texSet });
    
    return loadedTextures.size() - 1;
}

Texture VulkanEngine::upload_texture(const std::filesystem::path& path)
{
    RawImage rawImg;
    auto rawImgOpt = RawImage::load(path);
    if (!rawImgOpt.has_value())
    {
        std::println(stderr, "WARNING: {}. Using fallback white texture.", rawImgOpt.error());
        rawImg = RawImage::create_fallback_white(); 
    }
    else
    {
        rawImg = std::move(rawImgOpt.value());
    }
    auto& ctx = internal::context;
    
    Buffer stagingBuffer = Buffer::create(
        ctx.allocator,
        rawImg.pixels.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO
    );
    stagingBuffer.upload_data<std::byte>(ctx.allocator, rawImg.pixels);

    Texture tex;
    VkImageCreateInfo imageInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = { rawImg.width, rawImg.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VmaAllocationCreateInfo dmaAllocInfo { .usage = VMA_MEMORY_USAGE_AUTO };
    vmaCreateImage(ctx.allocator, &imageInfo, &dmaAllocInfo, &tex.image, &tex.allocation, nullptr);


    transferContext.execute(
        ctx.graphics_queue,
        [&](VkCommandBuffer cmd)
        {
            VkImageMemoryBarrier barrier
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = tex.image,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
            };
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy region
            {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset = { 0, 0, 0 },
                .imageExtent = { rawImg.width, rawImg.height, 1 }
            };
            vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    );

    VkImageViewCreateInfo viewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = tex.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCreateImageView(ctx.device, &viewInfo, nullptr, &tex.imageView);

    VkSamplerCreateInfo samplerInfo
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 16.0f
    };
    vkCreateSampler(ctx.device, &samplerInfo, nullptr, &tex.sampler);

    return tex; 
}

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

    geometry::generate_cone_mesh(coneMesh);
    geometry::generate_plane_mesh(planeMesh);

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
    
    transferContext.init(internal::context.graphics_queue_index);

    Shader mainVertShader(std::filesystem::current_path() / "shaders" / "main.vert.spv");
    Shader mainFragShader(std::filesystem::current_path() / "shaders" / "main.frag.spv");
    if (!mainVertShader.is_load() || !mainFragShader.is_load())
    {
        std::println(stderr, "ERROR: Failed to load shaders! Check your file paths.");
        return;
    }

    VkPushConstantRange pushConstantRange
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // TODO: vertex_bit?
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkDescriptorSetLayoutBinding texBinding
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutCreateInfo texLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &texBinding
    };
    vkCreateDescriptorSetLayout(internal::context.device, &texLayoutInfo, nullptr, &textureSetLayout);

    VkDescriptorSetLayout layouts[] { sceneSetLayout, objectSetLayout, textureSetLayout };
    VkPipelineLayoutCreateInfo layoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = sizeof(layouts) / sizeof(layouts[0]),
        .pSetLayouts = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
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
    VkVertexInputAttributeDescription attrDesc[3]
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
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, uv)
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
    builder.set_shaders(mainVertShader, mainFragShader);
    mainPipeline = builder.build_pipeline(internal::context.device);

    builder.set_shaders(mainVertShader, mainFragShader)
           .set_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
           .set_polygon_mode(VK_POLYGON_MODE_FILL); 
    linePipeline = builder.build_pipeline(internal::context.device);
    
    load_texture("", "Default White");
}

void VulkanEngine::cleanup()
{
    VkDevice device = graphics::internal::context.device;

    if (mainPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, mainPipeline, nullptr);
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
    transferContext.cleanup();

    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
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


    if (mainPipeline == VK_NULL_HANDLE)
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

    auto draw_batch = [&](
        VkPipeline pipeline,
        std::span<const RenderObjectData> render_objects,
        PushConstants& pc
    )
    {
        if (render_objects.empty()) { return; }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        const Mesh* lastMesh = nullptr;

        for (const auto& obj : render_objects)
        {
            if (current_dynamic_index >= maxObjects) { break; }

            pc.enableComplexSampling = obj.useComplexSampling ? 1U : 0U;
            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // TODO: vertex?
                0,
                sizeof(PushConstants),
                &pc
            );

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
            std::memcpy(
                pMappedData + (current_dynamic_index * dynamicAlignment),
                &objUbo,
                sizeof(ObjectUniform)
            );

            uint32_t dynamicOffset { static_cast<uint32_t>(current_dynamic_index * dynamicAlignment) };

            VkDescriptorSet sets[]
            {
                sceneDescriptorSet,
                dynamicObjectDescriptorSet,
                obj.textureDescriptorSet
            };
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

    PushConstants litPC
    {
        .isUnlit = 0,
        .time = static_cast<float>(data.time),
        .enableComplexSampling = 0
    };
    draw_batch(mainPipeline, data.litObjects, litPC);

    PushConstants unlitPC
    { 
        .isUnlit = 1,
        .time = static_cast<float>(data.time),
        .enableComplexSampling = 0
    };
    draw_batch(mainPipeline, data.unlitObjects, unlitPC);

    draw_batch(linePipeline, data.lineObjects, unlitPC);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core