#include "gpu_resources.hpp"

#include <cstdint>
#include <print>
#include <utility>
#include <fstream>

namespace graphics::core
{

Buffer::Buffer(Buffer&& other) noexcept
{
    std::swap(buffer, other.buffer);
    std::swap(allocation, other.allocation);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    destroy();
    std::swap(buffer, other.buffer);
    std::swap(allocation, other.allocation);
    return *this;
}

Buffer::~Buffer() { destroy(); };


Buffer Buffer::create(
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

void Buffer::destroy()
{
    if (buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(graphics::internal::context.allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
    }
}





Shader::~Shader() { destroy(); };

Shader::Shader(Shader&& other) noexcept
{
    std::swap(shader, other.shader);
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    destroy();
    std::swap(shader, other.shader);
    return *this;
}

Shader Shader::create(
    std::filesystem::path&& file_path
) {
    Shader sh(file_path);
    return sh;
}


bool Shader::is_load() const noexcept { return shader != VK_NULL_HANDLE; }

VkShaderModule Shader::load_shader_module(
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

void Shader::destroy()
{
    if (shader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(internal::context.device, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }
}





DescriptorBuilder& DescriptorBuilder::bind_buffer(
    uint32_t binding,
    VkDescriptorType type,
    VkShaderStageFlags stageFlags,
    VkDescriptorBufferInfo bufferInfo
) {
    bindings_.push_back(
        VkDescriptorSetLayoutBinding 
        {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = stageFlags,
            .pImmutableSamplers = nullptr
        }
    );

    bufferInfos_.push_back(std::move(bufferInfo));

    writes_.push_back(
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = type,
            .pBufferInfo = &bufferInfos_.back(),
        }
    );

    return *this;
}


DescriptorBuilder& DescriptorBuilder::bind_image(
    uint32_t binding,
    VkDescriptorType type,
    VkShaderStageFlags stageFlags,
    VkDescriptorImageInfo imageInfo
) {
    bindings_.push_back(
        VkDescriptorSetLayoutBinding 
        {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = stageFlags,
            .pImmutableSamplers = nullptr
        }
    );

    imageInfos_.push_back(imageInfo);

    writes_.push_back(
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = &imageInfos_.back(),
        }
    );

    return *this;
}    

bool DescriptorBuilder::build(
    VkDevice device,
    DescriptorAllocator& allocator,
    VkDescriptorSetLayout& outLayout,
    VkDescriptorSet& outSet
) {
    VkDescriptorSetLayoutCreateInfo layoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings_.size()),
        .pBindings = bindings_.data()
    };
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &outLayout) != VK_SUCCESS)
    {
        return false;
    }

    if (!allocator.allocate(outSet, outLayout))
    {
        return false;
    }
    
    for (auto& write : writes_) { write.dstSet = outSet; }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes_.size()), writes_.data(), 0, nullptr);

    clear();
    return true;
}

bool DescriptorBuilder::build_existing_layout(
    VkDevice device,
    DescriptorAllocator& allocator,
    VkDescriptorSetLayout layout,
    VkDescriptorSet& outSet
) {
    if (!allocator.allocate(outSet, layout))
    {
        return false;
    }
    
    for (auto& write : writes_) { write.dstSet = outSet; }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes_.size()), writes_.data(), 0, nullptr);

    clear();
    return true;
}

void DescriptorBuilder::clear()
{
    bindings_.clear();
    writes_.clear();
    bufferInfos_.clear();
    imageInfos_.clear();
}





void DescriptorAllocator::init(VkDevice dev)
{
    device = dev;
}

void DescriptorAllocator::cleanup()
{
    for (auto p : freePools) { vkDestroyDescriptorPool(device, p, nullptr); }
    for (auto p : usedPools) { vkDestroyDescriptorPool(device, p, nullptr); }
    if (currentPool) { vkDestroyDescriptorPool(device, currentPool, nullptr); }
}

void DescriptorAllocator::reset_pools()
{
    for (auto p : usedPools)
    {
        vkResetDescriptorPool(device, p, 0);
        freePools.push_back(p);
    }
    usedPools.clear();
    
    if (currentPool)
    {
        vkResetDescriptorPool(device, currentPool, 0);
        freePools.push_back(currentPool);
        currentPool = VK_NULL_HANDLE;
    }
}

VkDescriptorPool DescriptorAllocator::create_pool(
    uint32_t count,
    VkDescriptorPoolCreateFlags flags
) {
    std::vector<VkDescriptorPoolSize> sizes{};
    sizes.reserve(descriptorSizes.sizes.size());
    for (auto sz : descriptorSizes.sizes)
    {
        sizes.push_back({ sz.first, static_cast<uint32_t>(sz.second * count) });
    }

    VkDescriptorPoolCreateInfo pool_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = flags,
        .maxSets = count,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data()
    };

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);
    return descriptorPool;
}

VkDescriptorPool DescriptorAllocator::grab_pool()
{
    if (!freePools.empty())
    {
        VkDescriptorPool pool { freePools.back() };
        freePools.pop_back();
        return pool;
    }
    else
    {
        return create_pool(1000, 0);
    }
}

bool DescriptorAllocator::allocate(
    VkDescriptorSet& set,
    VkDescriptorSetLayout layout
) {
    if (currentPool == VK_NULL_HANDLE)
    {
        currentPool = grab_pool();
    }

    VkDescriptorSetAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = currentPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkResult allocResult { vkAllocateDescriptorSets(device, &allocInfo, &set) };
    
    switch (allocResult)
    {
        case VK_SUCCESS:
        {
            return true;
        }
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
        {
            usedPools.push_back(currentPool);
            currentPool = grab_pool();
            allocInfo.descriptorPool = currentPool;
            if (vkAllocateDescriptorSets(device, &allocInfo, &set) == VK_SUCCESS)
            {
                return true;
            }
            return false;
        }
        default:
        {
            return false;
        }
    }
}


} // namespace graphics::core
