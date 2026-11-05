#pragma once

#include "common.hpp"
#include "gpu_resources.hpp"

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace graphics::core
{

class VulkanEngine final
{
public:
    VulkanEngine() = default;
    ~VulkanEngine();

public:
    VkPipeline mainPipeline;
    VkPipeline linePipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorSet desriptorSet;
    VkDescriptorSetLayout desriptorSetLayout;
    DescriptorAllocator descriptorAllocator;

    VkDescriptorSetLayout sceneSetLayout; // Set 0
    VkDescriptorSetLayout objectSetLayout; // Set 1
    
    VkDescriptorSet sceneDescriptorSet;

    Texture whiteTexture; 
    Texture blackTexture;
    TransferContext transferContext;
    VkDescriptorSetLayout textureSetLayout;
    std::vector<MaterialResource> loadedMaterials;
    Texture upload_texture(const RawImage& rawImg);
    Texture upload_texture_from_file(
        const std::filesystem::path& path
    );
    size_t load_material(
        std::string name,
        std::filesystem::path albedo,
        std::filesystem::path specular = "",
        std::filesystem::path emissive = ""
    );

    Mesh cubeMesh;
    Mesh coneMesh;
    Mesh planeMesh;
    void upload_mesh(
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices,
        Mesh& outMesh
    );

    static constexpr size_t maxObjects { consts::maxObjects };
    size_t dynamicAlignment { 0uz };

    Buffer dynamicObjectBuffer;
    VkDescriptorSet dynamicObjectDescriptorSet;

    static constexpr size_t maxPointLights { consts::maxPointLights };
    Buffer pointLightBuffer;
    static constexpr size_t maxSpotLights { consts::maxSpotLights };
    Buffer spotLightBuffer;
    
    Buffer sceneUniformBuffer;
    
    void init();
    void cleanup();
    void draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, const FrameRenderData& data);
private:
    const std::array<Vertex, 24> vertices { detail::vertices };
    const std::array<uint32_t, 36> indices { detail::indices };
    
    void init_buffers();
    void init_descriptors();
};


} // namespace graphics::core
