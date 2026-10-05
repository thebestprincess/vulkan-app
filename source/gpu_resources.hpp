#pragma once

#include "graphics_internal.hpp"

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <cstring>
#include <utility>
#include <print>

namespace graphics::core
{

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
    void destroy() {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(graphics::internal::context.allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
        }
    }
};



} // namespace graphics::core