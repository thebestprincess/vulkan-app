#pragma once

#include "common.hpp"
#include "gpu_resources.hpp"

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace graphics::core
{

struct Transform final
{
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 get_matrix() const
    {
        glm::mat4 mat = glm::translate(glm::mat4(1.0f), position);
        mat = glm::rotate(mat, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        mat = glm::rotate(mat, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        mat = glm::rotate(mat, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        mat = glm::scale(mat, scale);
        return mat;
    }
};

struct RenderObjectData final
{
    glm::mat4 modelMatrix;
    Material material;
};

struct FrameRenderData final
{
    SceneUniform sceneUniform;
    std::span<const PointLight> pointLights;
    std::span<const SpotLight> spotLights;
    std::span<const RenderObjectData> objects;
};


class VulkanEngine final
{
public:
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkDescriptorSetLayout desriptorSetLayout;
    VkDescriptorSet desriptorSet;
    DescriptorAllocator descriptorAllocator;

    VkDescriptorSetLayout sceneSetLayout; // Set 0
    VkDescriptorSetLayout objectSetLayout; // Set 1
    
    VkDescriptorSet sceneDescriptorSet;
    
    Buffer vertexBuffer;
    Buffer indexBuffer;

    static constexpr size_t maxPointLights { 100uz };
    Buffer pointLightBuffer;
    static constexpr size_t maxSpotLights { 100uz };
    Buffer spotLightBuffer;
    
    Buffer sceneUniformBuffer;
    
    Buffer objectUniformBuffer;
    VkDescriptorSet objectDescriptorSet;

    Buffer object2UniformBuffer;
    VkDescriptorSet object2DescriptorSet;

    void init();
    void cleanup();
    void draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, const FrameRenderData& data);
private:
        const Vertex vertices[24] = {
        // Front - Normal: (0, 0, 1)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 0
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 1
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 2
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 3

        // Back - Normal (0, 0, -1)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 4
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 5
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 6
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 7

        // Left - Normal: (-1, 0, 0)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}}, // 8
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}}, // 9
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}}, // 10
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}}, // 11

        // Right - Normal: (1, 0, 0)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 12
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 13
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 14
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 15

        // Top - Normal: (0, 1, 0)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // 16
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // 17
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 18
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 19

        // Bottom - Normal: (0, -1, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}}, // 20
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}}, // 21
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}}, // 22
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}}  // 23
    };

    const uint32_t indices[36] = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };

    
    void init_buffers();
    void init_descriptors();
};

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
    VkFormat _colorAttachmentformat;

	PipelineBuilder(){ setup(); }
    VkPipeline build_pipeline(VkDevice device);
    
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

private:
    void setup();
};

} // namespace graphics::core
