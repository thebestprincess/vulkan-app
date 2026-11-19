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

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

MeshData generate_cone_mesh()
{
    MeshData data{};

    data.vertices.push_back({
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f),
        glm::vec2(0.0f)
    }); 

    const int segments { 16 };
    for (int i { 0 }; i < segments; ++i)
    {
        float angle { static_cast<float>(i) / segments * 2.0f * glm::pi<float>() };
        data.vertices.push_back({
            glm::vec3(std::cos(angle), std::sin(angle), -1.0f), glm::vec3(0.0f),
            glm::vec2(std::cos(angle)*0.5f + 0.5f, std::sin(angle)*0.5f + 0.5f)
        });
    }

    for (int i { 1 }; i <= segments; ++i)
    {
        int next { (i % segments) + 1 };
        data.indices.push_back(0);
        data.indices.push_back(i);
        data.indices.push_back(i);
        data.indices.push_back(next);
    }

    return data;
}

MeshData generate_plane_mesh()
{
    MeshData data;
    data.vertices = {
        {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
    };
    data.indices = { 0, 2, 1, 2, 0, 3 };

    return data;
}

} // namespace graphics::geometry

namespace graphics::core
{
VulkanEngine::~VulkanEngine() { cleanup(); }

size_t VulkanEngine::load_material(
    std::string name,
    std::filesystem::path albedo,
    std::filesystem::path specular,
    std::filesystem::path emissive
)
{
    MaterialResource mat;
    mat.name = std::move(name);

    mat.albedo = upload_texture_from_file(albedo);
    mat.specular = upload_texture_from_file(specular);
    mat.emissive = upload_texture_from_file(emissive);

    auto get_info = [](const Texture& tex, const Texture& fallback) -> VkDescriptorImageInfo
    {
        const Texture& t = (tex.image != VK_NULL_HANDLE) ? tex : fallback;
        return { t.sampler, t.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    };

    VkDescriptorImageInfo albedoInfo = get_info(mat.albedo, whiteTexture);
    VkDescriptorImageInfo specInfo = get_info(mat.specular, blackTexture);
    VkDescriptorImageInfo emissInfo = get_info(mat.emissive, blackTexture);

    DescriptorBuilder builder;
    builder.bind_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, albedoInfo)
           .bind_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, specInfo)
           .bind_image(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, emissInfo)
           .build_existing_layout(internal::context.device, descriptorAllocator, textureSetLayout, mat.descriptorSet);

    loadedMaterials.push_back(std::move(mat));
    return loadedMaterials.size() - 1;
}

Texture VulkanEngine::upload_texture_from_file(
    const std::filesystem::path& path
)
{
    if (path.empty())
    {
        return Texture{};
    }

    auto rawImgOpt { RawImage::load(path) };
    if (!rawImgOpt.has_value())
    {
        std::println(stderr, "Warning: {}. Using fallback.", rawImgOpt.error());
        return Texture{}; 
    }

    return upload_texture(rawImgOpt.value());
}


Texture VulkanEngine::upload_texture(const RawImage& rawImg)
{
    auto& ctx = internal::context;

    VmaAllocationCreateFlags mappedFlags
    {
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    };   
    Buffer stagingBuffer = Buffer::create(
        ctx.allocator,
        rawImg.pixels.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
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

void VulkanEngine::upload_mesh(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices,
    Mesh& outMesh
)
{
    auto& ctx { internal::context };
    
    VmaAllocationCreateFlags mappedFlags
    {
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    };

    Buffer stagingVerts = Buffer::create(
        ctx.allocator,
        vertices.size_bytes(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    );
    stagingVerts.upload_data(ctx.allocator, vertices);

    Buffer stagingIndices = Buffer::create(
        ctx.allocator,
        indices.size_bytes(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    );
    stagingIndices.upload_data(ctx.allocator, indices);

    outMesh.vertexBuffer = Buffer::create(
        ctx.allocator,
        vertices.size_bytes(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO,
        0
    );

    outMesh.indexBuffer = Buffer::create(
        ctx.allocator,
        indices.size_bytes(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO,
        0
    );
    outMesh.indexCount = static_cast<uint32_t>(indices.size());

    transferContext.execute(
        ctx.graphics_queue,
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy vertCopy { .srcOffset = 0, .dstOffset = 0, .size = vertices.size_bytes() };
            vkCmdCopyBuffer(cmd, stagingVerts.buffer, outMesh.vertexBuffer.buffer, 1, &vertCopy);

            VkBufferCopy indCopy { .srcOffset = 0, .dstOffset = 0, .size = indices.size_bytes() };
            vkCmdCopyBuffer(cmd, stagingIndices.buffer, outMesh.indexBuffer.buffer, 1, &indCopy);
        }
    );
}

void VulkanEngine::init_buffers()
{
    const auto& ctx = internal::context;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &properties);
    size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;

    dynamicAlignment = sizeof(ObjectUniform);
    if (minUboAlignment > 0)
    {
        dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    VmaAllocationCreateFlags mappedFlags 
    {
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    };

    dynamicObjectBuffer = Buffer::create(
        ctx.allocator,
        maxObjects * dynamicAlignment,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    );
    sceneUniformBuffer = Buffer::create(
        ctx.allocator,
        sizeof(SceneUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    ); 
    pointLightBuffer = Buffer::create(
        ctx.allocator, 
        sizeof(PointLight) * maxPointLights, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    );
    spotLightBuffer = Buffer::create(
        ctx.allocator,
        sizeof(SpotLight) * maxSpotLights, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        mappedFlags
    );

    upload_mesh(vertices, indices, cubeMesh);

    auto coneData = geometry::generate_cone_mesh();
    upload_mesh(coneData.vertices, coneData.indices, coneMesh);

    auto planeData = geometry::generate_plane_mesh();
    upload_mesh(planeData.vertices, planeData.indices, planeMesh);
}

void VulkanEngine::init_descriptors()
{
    VkDevice device = graphics::internal::context.device;
    DescriptorBuilder builder;

    VkDescriptorBufferInfo sceneInfo { sceneUniformBuffer, 0, sizeof(SceneUniform) };
    VkDescriptorBufferInfo pointLightInfo { pointLightBuffer, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo spotLightInfo { spotLightBuffer, 0, VK_WHOLE_SIZE };

    VkDescriptorImageInfo dirShadowInfo = dirShadowMap.descriptor_info();
    VkDescriptorImageInfo spotShadowInfo = spotShadowMaps.descriptor_info();

    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sceneInfo)
           .bind_buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, pointLightInfo)
           .bind_buffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, spotLightInfo)
           .bind_image(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, dirShadowInfo)
           .bind_image(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, spotShadowInfo)
           .build(device, descriptorAllocator, sceneSetLayout, sceneDescriptorSet);

    VkDescriptorBufferInfo dynObjInfo { dynamicObjectBuffer, 0, sizeof(ObjectUniform) };
    builder.bind_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, dynObjInfo)
           .build(device, descriptorAllocator, objectSetLayout, dynamicObjectDescriptorSet);
}


void VulkanEngine::init()
{
    descriptorAllocator.init(graphics::internal::context.device);
    transferContext.init(internal::context.graphics_queue_index);

    dirShadowMap = ShadowMap::create(shadowMapSize, shadowMapSize);
    spotShadowMaps = ShadowMapArray::create(
        shadowMapSize,
        shadowMapSize,
        static_cast<uint32_t>(consts::maxSpotShadowLights)
    );

    init_buffers();
    init_descriptors();

    whiteTexture = upload_texture(RawImage::create_solid(255, 255, 255, 255));
    blackTexture = upload_texture(RawImage::create_solid(0, 0, 0, 255));

    Shader mainVertShader(std::filesystem::current_path() / "shaders" / "main.vert.spv");
    Shader mainFragShader(std::filesystem::current_path() / "shaders" / "main.frag.spv");
    Shader shadowVertShader(std::filesystem::current_path() / "shaders" / "shadow.vert.spv");
    
    if (!mainVertShader.is_load() || !mainFragShader.is_load() || !shadowVertShader.is_load())
    {
        std::println(stderr, "ERROR: Failed to load shaders! Check your file paths.");
        return;
    }

    VkPushConstantRange pushConstantRange
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkDescriptorSetLayoutBinding texBindings[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        texBindings[i] = {
            .binding = i,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };
    }
    VkDescriptorSetLayoutCreateInfo texLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = texBindings
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
            .format = VK_FORMAT_R32G32_SFLOAT,
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

    // Shadow Pipeline
    VkPushConstantRange shadowPushRange
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(ShadowPushConstants)
    };
    VkDescriptorSetLayout shadowLayouts[] { sceneSetLayout, objectSetLayout };
    VkPipelineLayoutCreateInfo shadowLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = sizeof(shadowLayouts) / sizeof(shadowLayouts[0]),
        .pSetLayouts = shadowLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &shadowPushRange
    };
    vkCreatePipelineLayout(internal::context.device, &shadowLayoutInfo, nullptr, &shadowPipelineLayout);

    VkVertexInputAttributeDescription shadowAttrDesc[]
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position)
        }
    };

    PipelineBuilder shadowBuilder;
    shadowBuilder._pipelineLayout = shadowPipelineLayout;
    shadowBuilder._vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = sizeof(bindingDesc) / sizeof(bindingDesc[0]),
        .pVertexBindingDescriptions = bindingDesc,
        .vertexAttributeDescriptionCount = sizeof(shadowAttrDesc) / sizeof(shadowAttrDesc[0]),
        .pVertexAttributeDescriptions = shadowAttrDesc
    };
    shadowBuilder.set_vertex_shader(shadowVertShader)
                 .set_cull_mode(VK_CULL_MODE_BACK_BIT)
                 .enable_depth_bias(true)
                 .set_dynamic_rendering(dirShadowMap.format);
    shadowPipeline = shadowBuilder.build_pipeline(internal::context.device);
}

void VulkanEngine::cleanup()
{
    VkDevice device = graphics::internal::context.device;

    if (mainPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, mainPipeline, nullptr);
        mainPipeline = VK_NULL_HANDLE;
    }
    if (linePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, linePipeline, nullptr);
        linePipeline = VK_NULL_HANDLE;
    }
    if (shadowPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, shadowPipeline, nullptr);
        shadowPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (shadowPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
        shadowPipelineLayout = VK_NULL_HANDLE;
    }

    descriptorAllocator.cleanup();
    transferContext.cleanup();

    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
}

void VulkanEngine::draw(
    VkCommandBuffer cmd,
    VkFramebuffer framebuffer,
    const FrameRenderData& data
)
{
    auto& ctx = graphics::internal::context;

    VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) { return; }

    SceneUniform sceneUbo { data.sceneUniform };
    sceneUniformBuffer.upload_data<SceneUniform>(ctx.allocator, std::span{&sceneUbo, 1});

    if (!data.pointLights.empty())
    {
        pointLightBuffer.upload_data<PointLight>(ctx.allocator, data.pointLights);
    }
    if (!data.spotLights.empty())
    {
        spotLightBuffer.upload_data<SpotLight>(ctx.allocator, data.spotLights);
    }

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(ctx.allocator, dynamicObjectBuffer.allocation, &allocInfo);
    char* pMappedData { static_cast<char*>(allocInfo.pMappedData) };

    const size_t litCount { std::min(data.litObjects.size(), maxObjects) };
    for (size_t i { 0uz }; i < litCount; ++i)
    {
        const auto& obj = data.litObjects[i];
        ObjectUniform objUbo
        {
            obj.modelMatrix,
            obj.normalMatrix,
            obj.material
        };
        std::memcpy(
            pMappedData + (i * dynamicAlignment),
            &objUbo,
            sizeof(ObjectUniform)
        );
    }

    auto render_shadow_pass = [&](
        ShadowMap& shadowMap,
        const glm::mat4& lightViewProj,
        bool renderGeometry
    )
    {
        shadowMap.transition_to_depth_attachment(cmd);

        const VkRenderingAttachmentInfo depthAttachment
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = shadowMap.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .depthStencil = { 1.0f, 0 } },
        };

        const VkRenderingInfo renderingInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { 0, 0 }, shadowMap.extent },
            .layerCount = 1,
            .colorAttachmentCount = 0,
            .pColorAttachments = nullptr,
            .pDepthAttachment = &depthAttachment,
        };

        vkCmdBeginRendering(cmd, &renderingInfo);

        if (renderGeometry && shadowPipeline != VK_NULL_HANDLE && litCount > 0)
        {
            const VkViewport shadowViewport
            {
                0.0f, 0.0f,
                static_cast<float>(shadowMap.extent.width),
                static_cast<float>(shadowMap.extent.height),
                0.0f, 1.0f
            };
            const VkRect2D shadowScissor { { 0, 0 }, shadowMap.extent };
            vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
            vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
            vkCmdSetDepthBias(
                cmd,
                data.shadowSettings.depthBiasConstant,
                0.0f,
                data.shadowSettings.depthBiasSlope
            );

            const ShadowPushConstants shadowPC { .lightViewProj = lightViewProj };
            vkCmdPushConstants(
                cmd,
                shadowPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(ShadowPushConstants),
                &shadowPC
            );

            const Mesh* lastMesh = nullptr;
            for (size_t i { 0uz }; i < litCount; ++i)
            {
                const auto& obj = data.litObjects[i];
                if (lastMesh != obj.mesh)
                {
                    VkDeviceSize offset { 0 };
                    vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->vertexBuffer.buffer, &offset);
                    vkCmdBindIndexBuffer(cmd, obj.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                    lastMesh = obj.mesh;
                }

                uint32_t dynamicOffset { static_cast<uint32_t>(i * dynamicAlignment) };
                VkDescriptorSet sets[] { sceneDescriptorSet, dynamicObjectDescriptorSet };
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    shadowPipelineLayout,
                    0,
                    sizeof(sets) / sizeof(sets[0]),
                    sets,
                    1,
                    &dynamicOffset
                );

                vkCmdDrawIndexed(cmd, obj.mesh->indexCount, 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);
        shadowMap.transition_to_shader_read(cmd);
    };

    render_shadow_pass(
        dirShadowMap,
        data.shadowSettings.dirLightSpaceMatrix,
        data.shadowSettings.enableDirShadow
    );

    auto render_shadow_target = [&](
        VkImageView depthView,
        VkExtent2D extent,
        const glm::mat4& lightViewProj,
        bool renderGeometry
    )
    {
        const VkRenderingAttachmentInfo depthAttachment
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .depthStencil = { 1.0f, 0 } },
        };

        const VkRenderingInfo renderingInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { 0, 0 }, extent },
            .layerCount = 1,
            .colorAttachmentCount = 0,
            .pColorAttachments = nullptr,
            .pDepthAttachment = &depthAttachment,
        };

        vkCmdBeginRendering(cmd, &renderingInfo);

        if (renderGeometry && shadowPipeline != VK_NULL_HANDLE && litCount > 0)
        {
            const VkViewport shadowViewport
            {
                0.0f, 0.0f,
                static_cast<float>(extent.width),
                static_cast<float>(extent.height),
                0.0f, 1.0f
            };
            const VkRect2D shadowScissor { { 0, 0 }, extent };
            vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
            vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
            vkCmdSetDepthBias(
                cmd,
                data.shadowSettings.depthBiasConstant,
                0.0f,
                data.shadowSettings.depthBiasSlope
            );

            const ShadowPushConstants shadowPC { .lightViewProj = lightViewProj };
            vkCmdPushConstants(
                cmd,
                shadowPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(ShadowPushConstants),
                &shadowPC
            );

            const Mesh* lastMesh = nullptr;
            for (size_t i { 0uz }; i < litCount; ++i)
            {
                const auto& obj = data.litObjects[i];
                if (lastMesh != obj.mesh)
                {
                    VkDeviceSize offset { 0 };
                    vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->vertexBuffer.buffer, &offset);
                    vkCmdBindIndexBuffer(cmd, obj.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                    lastMesh = obj.mesh;
                }

                uint32_t dynamicOffset { static_cast<uint32_t>(i * dynamicAlignment) };
                VkDescriptorSet sets[] { sceneDescriptorSet, dynamicObjectDescriptorSet };
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    shadowPipelineLayout,
                    0,
                    sizeof(sets) / sizeof(sets[0]),
                    sets,
                    1,
                    &dynamicOffset
                );

                vkCmdDrawIndexed(cmd, obj.mesh->indexCount, 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);
    };

    dirShadowMap.transition_to_depth_attachment(cmd);
    render_shadow_target(
        dirShadowMap.imageView,
        dirShadowMap.extent,
        data.shadowSettings.dirLightSpaceMatrix,
        data.shadowSettings.enableDirShadow
    );
    dirShadowMap.transition_to_shader_read(cmd);

    spotShadowMaps.transition_to_depth_attachment(cmd);
    const uint32_t spotShadowCount = data.shadowSettings.enableSpotShadows
        ? std::min(data.sceneUniform.spotShadowCount, spotShadowMaps.layerCount)
        : 0u;
    for (uint32_t layer = 0; layer < spotShadowCount; ++layer)
    {
        render_shadow_target(
            spotShadowMaps.layerImageViews[layer],
            spotShadowMaps.extent,
            data.shadowSettings.spotLightSpaceMatrices[layer],
            true
        );
    }
    spotShadowMaps.transition_to_shader_read(cmd);

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

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
        static_cast<float>(ctx.swapchain_extent.width),
        static_cast<float>(ctx.swapchain_extent.height),
        0.0f, 1.0f
    };
    VkRect2D scissor
    {
        {0, 0},
        ctx.swapchain_extent
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    size_t current_dynamic_index { litCount };

    auto draw_batch = [&](
        VkPipeline pipeline,
        std::span<const RenderObjectData> render_objects,
        PushConstants& pc,
        bool usePreuploadedRange
    )
    {
        if (render_objects.empty()) { return; }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        const Mesh* lastMesh = nullptr;

        for (size_t i { 0uz }; i < render_objects.size(); ++i)
        {
            const size_t slotIndex = usePreuploadedRange ? i : current_dynamic_index;
            if (slotIndex >= maxObjects) { break; }

            const auto& obj = render_objects[i];

            pc.enableComplexSampling = obj.useComplexSampling ? 1U : 0U;
            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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

            if (!usePreuploadedRange)
            {
                ObjectUniform objUbo
                {
                    obj.modelMatrix,
                    obj.normalMatrix,
                    obj.material
                };
                std::memcpy(
                    pMappedData + (slotIndex * dynamicAlignment),
                    &objUbo,
                    sizeof(ObjectUniform)
                );
                ++current_dynamic_index;
            }

            uint32_t dynamicOffset { static_cast<uint32_t>(slotIndex * dynamicAlignment) };

            VkDescriptorSet sets[]
            {
                sceneDescriptorSet,
                dynamicObjectDescriptorSet,
                obj.materialDescriptorSet
            };
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                sizeof(sets) / sizeof(sets[0]),
                sets,
                1,
                &dynamicOffset
            );

            vkCmdDrawIndexed(cmd, obj.mesh->indexCount, 1, 0, 0, 0);
        }
    };

    PushConstants litPC
    {
        .isUnlit = 0,
        .time = static_cast<float>(data.time),
        .enableComplexSampling = 0
    };
    draw_batch(mainPipeline, data.litObjects, litPC, true);

    PushConstants unlitPC
    {
        .isUnlit = 1,
        .time = static_cast<float>(data.time),
        .enableComplexSampling = 0
    };
    draw_batch(mainPipeline, data.unlitObjects, unlitPC, false);

    draw_batch(linePipeline, data.lineObjects, unlitPC, false);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

} // namespace graphics::core