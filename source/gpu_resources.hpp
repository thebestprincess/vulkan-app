#pragma once

#include "graphics_internal.hpp"

#include <cstdint>
#include <deque>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <cstring>
#include <filesystem>
#include <vector>
#include <expected>
#include <concepts>
#include <span>

namespace graphics::core
{

class Buffer final
{
public:
    VkBuffer buffer { VK_NULL_HANDLE };
    VmaAllocation allocation { VK_NULL_HANDLE };

    Buffer() = default;
    ~Buffer();
    
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;


    static Buffer create(
        VmaAllocator allocator,
        size_t allocSize,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage
    );

    template<typename T>
    void upload_data(
        VmaAllocator allocator,
        std::span<const T> data
    ) {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator, allocation, &allocInfo);
        std::memcpy(allocInfo.pMappedData, data.data(), data.size_bytes());
    }

    operator VkBuffer() const noexcept { return buffer; }

private:
    void destroy();
};

class Shader final
{
public:
    VkShaderModule shader;

    Shader() = default;
    Shader(std::filesystem::path file_path)
        : shader(load_shader_module(file_path)) {}

    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    operator VkShaderModule() const noexcept { return shader; }
    
    static Shader create(std::filesystem::path&& file_path);
    bool is_load() const noexcept;

private:
    static VkShaderModule load_shader_module(std::filesystem::path& path);
    void destroy();
};


class DescriptorAllocator
{
public:
    struct PoolSizes
    {
        std::vector<std::pair<VkDescriptorType, float>> sizes
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
        };
    };

    void init(VkDevice device);
    void cleanup();
    void reset_pools();
    bool allocate(VkDescriptorSet& set, VkDescriptorSetLayout layout);

private:
    VkDevice device{VK_NULL_HANDLE};
    VkDescriptorPool currentPool{VK_NULL_HANDLE};
    PoolSizes descriptorSizes;
    std::vector<VkDescriptorPool> usedPools;
    std::vector<VkDescriptorPool> freePools;

    VkDescriptorPool create_pool(
        uint32_t count,
        VkDescriptorPoolCreateFlags flags
    );
    VkDescriptorPool grab_pool();
};


class DescriptorBuilder final
{
    std::vector<VkDescriptorSetLayoutBinding> bindings_;
    std::vector<VkWriteDescriptorSet> writes_;

    // Using a deque to prevent invalidation caused by reallocation
    std::deque<VkDescriptorBufferInfo> bufferInfos_; 
    std::deque<VkDescriptorImageInfo> imageInfos_;

public:
    DescriptorBuilder() = default;

    DescriptorBuilder& bind_buffer(
        uint32_t binding,
        VkDescriptorType type,
        VkShaderStageFlags stageFlags,
        VkDescriptorBufferInfo bufferInfo
    );

    DescriptorBuilder& bind_image(
        uint32_t binding,
        VkDescriptorType type,
        VkShaderStageFlags stageFlags,
        VkDescriptorImageInfo imageInfo
    );

    bool build(
        VkDevice device,
        DescriptorAllocator& allocator,
        VkDescriptorSetLayout& outLayout,
        VkDescriptorSet& outSet
    );

    bool build_existing_layout(
        VkDevice device,
        DescriptorAllocator& allocator,
        VkDescriptorSetLayout layout,
        VkDescriptorSet& outSet
    );

    void clear();
};

class RawImage final
{
public:
    uint32_t width{0};
    uint32_t height{0};
    uint32_t channels{0};
    std::vector<std::byte> pixels;

    static auto load(const std::filesystem::path& path) -> std::expected<RawImage, std::string>;
    static RawImage create_solid(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
};

class TransferContext final
{
public:
    void init(uint32_t queueFamilyIndex);
    void cleanup();

    void execute(VkQueue queue, std::invocable<VkCommandBuffer> auto&& commands) 
    {
        vkResetCommandBuffer(cmdBuffer, 0);

        VkCommandBufferBeginInfo beginInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        commands(cmdBuffer);

        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer
        };

        vkQueueSubmit(queue, 1, &submitInfo, fence);
        vkWaitForFences(internal::context.device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(internal::context.device, 1, &fence);
    }

private:
    VkCommandPool commandPool { VK_NULL_HANDLE };
    VkCommandBuffer cmdBuffer { VK_NULL_HANDLE };
    VkFence fence { VK_NULL_HANDLE };
};


class Texture final
{
public:
    VkImage image { VK_NULL_HANDLE };
    VmaAllocation allocation { VK_NULL_HANDLE };
    VkImageView imageView { VK_NULL_HANDLE };
    VkSampler sampler { VK_NULL_HANDLE };

    Texture() = default;
    ~Texture();

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

private:
    void destroy();
};

} // namespace graphics::core