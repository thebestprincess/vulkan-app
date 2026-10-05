#pragma once

#include "gpu_resources.hpp"

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <deque>
#include <functional>
#include <ranges>
#include <filesystem>

namespace graphics::core::uitls
{

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors{};

	void push_function(std::function<void()>&& function)
    {
		deletors.push_back(std::move(function));
	}

	void flush()
    {
        auto local { std::move(deletors) };
        deletors.clear();

		// reverse iterate the deletion queue to execute all the functions
		for (auto& func : deletors | std::views::reverse)
        {
			func();
		}
	}

    VkShaderModule load_shader_module(std::filesystem::path& path) const;
};

} // namespace graphics::core::uitls

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
    glm::vec4 color;
};

struct FrameRenderData final
{
    glm::mat4 view;
    glm::mat4 proj;
    std::span<const RenderObjectData> objects;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct Vertex
{
    glm::vec3 position;
};

struct SceneUniform
{
    glm::mat4 view;
    glm::mat4 proj;
};

struct ObjectUniform
{
    glm::mat4 model;
    glm::vec4 color;
};

class VulkanEngine final
{
public:
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkDescriptorSetLayout desriptorSetLayout;
    VkDescriptorSet desriptorSet;
    VkDescriptorPool descriptorPool;

    VkDescriptorSetLayout cameraSetLayout; // Set 0
    VkDescriptorSetLayout objectSetLayout; // Set 1
    
    VkDescriptorSet cameraDescriptorSet;
    
    Buffer vertexBuffer;
    Buffer indexBuffer;
    
    Buffer sceneUniformBuffer;
    
    Buffer objectUniformBuffer;
    VkDescriptorSet objectDescriptorSet;

    Buffer object2UniformBuffer;
    VkDescriptorSet object2DescriptorSet;

    void init();
    void cleanup();
    void draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, const FrameRenderData& data);
private:
    const Vertex vertices[8]
    {
        {glm::vec3(-1.0f, -1.0f,  0.0f)},
        {glm::vec3( 1.0f, -1.0f,  0.0f)},
        {glm::vec3( 1.0f, -1.0f,  1.0f)},
        {glm::vec3(-1.0f, -1.0f,  1.0f)},
        {glm::vec3(-1.0f,  1.0f,  0.0f)},
        {glm::vec3( 1.0f,  1.0f,  0.0f)},
        {glm::vec3( 1.0f,  1.0f,  1.0f)},
        {glm::vec3(-1.0f,  1.0f,  1.0f)},
    };
    const uint32_t indices[36]
    {
        0, 1, 5, 0, 5, 4,
        3, 2, 6, 3, 6, 7,
        4, 5, 6, 4, 6, 7,
        0, 1, 2, 0, 2, 3,
        0, 4, 7, 0, 7, 3,
        1, 5, 6, 1, 6, 2
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
