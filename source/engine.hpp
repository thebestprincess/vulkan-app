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
    VkPipeline litPipeline;
    VkPipeline unlitPipeline;
    VkPipeline linePipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorSet desriptorSet;
    VkDescriptorSetLayout desriptorSetLayout;
    DescriptorAllocator descriptorAllocator;

    VkDescriptorSetLayout sceneSetLayout; // Set 0
    VkDescriptorSetLayout objectSetLayout; // Set 1
    
    VkDescriptorSet sceneDescriptorSet;
    
    Mesh cubeMesh;
    Mesh coneMesh;
    Mesh planeMesh;

    static constexpr size_t maxObjects { 1000uz }; // number of objects on the scene simultaneously
    size_t dynamicAlignment { 0uz };

    Buffer dynamicObjectBuffer;
    VkDescriptorSet dynamicObjectDescriptorSet;

    static constexpr size_t maxPointLights { 100uz };
    Buffer pointLightBuffer;
    static constexpr size_t maxSpotLights { 100uz };
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
    void generate_cone_mesh();
    void generate_plane_mesh();
};


} // namespace graphics::core
