#pragma once

#include "graphics_internal.hpp"

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <cstring>
#include <utility>
#include <print>
#include <filesystem>
#include <fstream>
#include <vector>

namespace graphics::core
{

namespace utils
{


} // namespace graphics::core::utils



class Buffer final
{
public:
    VkBuffer buffer { VK_NULL_HANDLE };
    VmaAllocation allocation { VK_NULL_HANDLE };

    Buffer() = default;
    
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
    {
        std::swap(buffer, other.buffer);
        std::swap(allocation, other.allocation);
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        destroy();
        std::swap(buffer, other.buffer);
        std::swap(allocation, other.allocation);
        return *this;
    }

    ~Buffer() { destroy(); };


    static Buffer create(
        VmaAllocator allocator,
        size_t allocSize,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage
    ) {
        Buffer b;
        VkBufferCreateInfo bufferInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = allocSize,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VmaAllocationCreateInfo vmaAllocInfo
        {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = memoryUsage,
        };

        if (vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo,
                            &b.buffer, &b.allocation, nullptr) != VK_SUCCESS)
        {
            std::println(stderr, "Failed to allocate buffer!");
        }

        return b;
    }

    template<typename T>
    void upload_data(
        VmaAllocator allocator,
        std::span<const T> data
    ) {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator, allocation, &allocInfo);
        std::memcpy(allocInfo.pMappedData, data.data(), data.size_bytes());
    }

private:
    void destroy()
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(graphics::internal::context.allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
        }
    }
};

class Shader final
{
public:
    VkShaderModule shader;

    Shader() = default;
    Shader(std::filesystem::path file_path)
        : shader(load_shader_module(file_path)) {}

    ~Shader() { destroy(); };

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept
    {
        std::swap(shader, other.shader);
    }
    Shader& operator=(Shader&& other) noexcept
    {
        destroy();
        std::swap(shader, other.shader);
        return *this;
    }

    static Shader create(
        std::filesystem::path&& file_path
    ) {
        Shader sh(file_path);
        return sh;
    }

    operator VkShaderModule() const noexcept { return shader; }

    bool is_load() const noexcept { return shader != VK_NULL_HANDLE; }

private:
    static VkShaderModule load_shader_module(
        std::filesystem::path& path
    ) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const std::streampos pos { file.tellg() };
        if (pos == std::streampos(-1)) return VK_NULL_HANDLE;

        const size_t size { static_cast<size_t>(pos) };
        std::vector<uint32_t> buffer(size / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        VkShaderModuleCreateInfo info
        {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = size,
            .pCode = buffer.data(),
        };

        VkShaderModule result;
        if (vkCreateShaderModule(graphics::internal::context.device,
                                &info, nullptr, &result) != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        } 

        return result;
    }

    void destroy()
    {
        if (shader != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(internal::context.device, shader, nullptr);
            shader = VK_NULL_HANDLE;
        }
    }
};



} // namespace graphics::core