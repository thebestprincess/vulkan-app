#pragma once

#include "graphics_internal.hpp"

#include <cstdint>
#include <deque>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <cstring>
#include <filesystem>
#include <vector>

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

    VkDescriptorPool create_pool(uint32_t count, VkDescriptorPoolCreateFlags flags);
    VkDescriptorPool grab_pool();
};


class DescriptorBuilder final
{
    std::vector<VkDescriptorSetLayoutBinding> bindings_;
    std::vector<VkWriteDescriptorSet> writes_;

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

} // namespace graphics::core