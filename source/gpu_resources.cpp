#include "gpu_resources.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <format>

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
    if (this != &other)
    {
        destroy();
        std::swap(buffer, other.buffer);
        std::swap(allocation, other.allocation);
    }
    return *this;
}

Buffer::~Buffer() { destroy(); };


Buffer Buffer::create(
    VmaAllocator allocator,
    size_t allocSize,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags allocFlags
)
{
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
        .flags = allocFlags,
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
    if (this != &other)
    {
        destroy();
        std::swap(shader, other.shader);
    }
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



auto RawImage::load(
    const std::filesystem::path& path
) -> std::expected<RawImage, std::string>
{
    int w{};
    int h{};
    int c{};
    stbi_uc* data { stbi_load(path.string().c_str(), &w, &h, &c, STBI_rgb_alpha) };
    if (!data)
    {
        return std::unexpected(std::format("Failed to load image: {}", path.string()));
    }

    RawImage img
    {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .channels = 4 // RGBA
    };
    
    size_t size { w * h * 4UZ };
    img.pixels.resize(size);
    std::memcpy(img.pixels.data(), data, size);
    
    stbi_image_free(data); // Free С-memory
    return img;
}

RawImage RawImage::create_solid(
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a
)
{
    RawImage img { .width = 1, .height = 1, .channels = 4 };
    img.pixels.push_back(static_cast<std::byte>(r));
    img.pixels.push_back(static_cast<std::byte>(g));
    img.pixels.push_back(static_cast<std::byte>(b));
    img.pixels.push_back(static_cast<std::byte>(a));
    return img;
}


void TransferContext::init(
    uint32_t queueFamilyIndex
) {
    auto& ctx = graphics::internal::context;

    VkCommandPoolCreateInfo poolInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };
    vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(ctx.device, &allocInfo, &cmdBuffer);

    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(ctx.device, &fenceInfo, nullptr, &fence);
}

void TransferContext::cleanup()
{
    auto& ctx = graphics::internal::context;
    vkDestroyFence(ctx.device, fence, nullptr);
    vkDestroyCommandPool(ctx.device, commandPool, nullptr);
}


Texture::~Texture()
{
    destroy();
}

Texture::Texture(
    Texture&& other
) noexcept
{
    std::swap(image, other.image);
    std::swap(allocation, other.allocation);
    std::swap(imageView, other.imageView);
    std::swap(sampler, other.sampler);
}

Texture& Texture::operator=(
    Texture&& other
) noexcept
{
    if (this != &other)
    {
        destroy();
        std::swap(image, other.image);
        std::swap(allocation, other.allocation);
        std::swap(imageView, other.imageView);
        std::swap(sampler, other.sampler);
    }

    return *this;
}

void Texture::destroy()
{
    auto& ctx = graphics::internal::context;
    if (sampler) vkDestroySampler(ctx.device, sampler, nullptr);
    if (imageView) vkDestroyImageView(ctx.device, imageView, nullptr);
    if (image) vmaDestroyImage(ctx.allocator, image, allocation);
    image = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
}

namespace
{

VkFormat select_shadow_depth_format(VkPhysicalDevice physicalDevice)
{
    constexpr VkFormat candidates[] {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };

    for (VkFormat fmt : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, fmt, &props);

        const bool supportsDepth =
            (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
        const bool supportsShadowSample =
            (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;

        if (supportsDepth && supportsShadowSample)
        {
            return fmt;
        }
    }

    return VK_FORMAT_D32_SFLOAT;
}

} // namespace

ShadowMap::~ShadowMap()
{
    destroy();
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
{
    std::swap(image, other.image);
    std::swap(allocation, other.allocation);
    std::swap(imageView, other.imageView);
    std::swap(sampler, other.sampler);
    std::swap(extent, other.extent);
    std::swap(format, other.format);
    std::swap(currentLayout, other.currentLayout);
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        std::swap(image, other.image);
        std::swap(allocation, other.allocation);
        std::swap(imageView, other.imageView);
        std::swap(sampler, other.sampler);
        std::swap(extent, other.extent);
        std::swap(format, other.format);
        std::swap(currentLayout, other.currentLayout);
    }
    return *this;
}

ShadowMap ShadowMap::create(uint32_t width, uint32_t height)
{
    auto& ctx = graphics::internal::context;

    ShadowMap sm;
    sm.extent = { width, height };
    sm.format = select_shadow_depth_format(ctx.physical_device);

    const VkImageCreateInfo imageInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = sm.format,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo allocInfo
    {
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    if (vmaCreateImage(ctx.allocator, &imageInfo, &allocInfo,
                       &sm.image, &sm.allocation, nullptr) != VK_SUCCESS)
    {
        std::println(stderr, "Failed to create shadow map image!");
        return sm;
    }

    const VkImageViewCreateInfo viewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = sm.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = sm.format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (vkCreateImageView(ctx.device, &viewInfo, nullptr, &sm.imageView) != VK_SUCCESS)
    {
        std::println(stderr, "Failed to create shadow map image view!");
    }

    const VkSamplerCreateInfo samplerInfo
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_TRUE,
        .compareOp = VK_COMPARE_OP_LESS,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    };

    if (vkCreateSampler(ctx.device, &samplerInfo, nullptr, &sm.sampler) != VK_SUCCESS)
    {
        std::println(stderr, "Failed to create shadow map sampler!");
    }

    return sm;
}

void ShadowMap::transition_to_depth_attachment(VkCommandBuffer cmd)
{
    const bool fromShaderRead = (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const VkPipelineStageFlags srcStage = fromShaderRead
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    const VkAccessFlags srcAccess = fromShaderRead
        ? VK_ACCESS_SHADER_READ_BIT
        : 0;

    const VkImageMemoryBarrier barrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void ShadowMap::transition_to_shader_read(VkCommandBuffer cmd)
{
    const VkImageMemoryBarrier barrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void ShadowMap::destroy()
{
    auto& ctx = graphics::internal::context;
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(ctx.device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
    if (imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(ctx.device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(ctx.allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

ShadowMapArray::~ShadowMapArray()
{
    destroy();
}

ShadowMapArray::ShadowMapArray(ShadowMapArray&& other) noexcept
{
    std::swap(image, other.image);
    std::swap(allocation, other.allocation);
    std::swap(arrayImageView, other.arrayImageView);
    std::swap(layerImageViews, other.layerImageViews);
    std::swap(sampler, other.sampler);
    std::swap(extent, other.extent);
    std::swap(format, other.format);
    std::swap(layerCount, other.layerCount);
    std::swap(currentLayout, other.currentLayout);
}

ShadowMapArray& ShadowMapArray::operator=(ShadowMapArray&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        std::swap(image, other.image);
        std::swap(allocation, other.allocation);
        std::swap(arrayImageView, other.arrayImageView);
        std::swap(layerImageViews, other.layerImageViews);
        std::swap(sampler, other.sampler);
        std::swap(extent, other.extent);
        std::swap(format, other.format);
        std::swap(layerCount, other.layerCount);
        std::swap(currentLayout, other.currentLayout);
    }
    return *this;
}

ShadowMapArray ShadowMapArray::create(uint32_t width, uint32_t height, uint32_t layers)
{
    auto& ctx = graphics::internal::context;

    ShadowMapArray sma;
    sma.extent = { width, height };
    sma.layerCount = layers;
    sma.format = select_shadow_depth_format(ctx.physical_device);

    const VkImageCreateInfo imageInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = sma.format,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo allocInfo
    {
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    if (vmaCreateImage(ctx.allocator, &imageInfo, &allocInfo,
                       &sma.image, &sma.allocation, nullptr) != VK_SUCCESS)
    {
        std::println(stderr, "Failed to create shadow map array image!");
        return sma;
    }

    const VkImageViewCreateInfo arrayViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = sma.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = sma.format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layers,
        },
    };
    vkCreateImageView(ctx.device, &arrayViewInfo, nullptr, &sma.arrayImageView);

    sma.layerImageViews.resize(layers, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < layers; ++i)
    {
        const VkImageViewCreateInfo layerViewInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sma.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = sma.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = i,
                .layerCount = 1,
            },
        };
        vkCreateImageView(ctx.device, &layerViewInfo, nullptr, &sma.layerImageViews[i]);
    }

    const VkSamplerCreateInfo samplerInfo
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_TRUE,
        .compareOp = VK_COMPARE_OP_LESS,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    };
    vkCreateSampler(ctx.device, &samplerInfo, nullptr, &sma.sampler);

    return sma;
}

void ShadowMapArray::transition_to_depth_attachment(VkCommandBuffer cmd)
{
    const bool fromShaderRead = (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const VkPipelineStageFlags srcStage = fromShaderRead
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    const VkAccessFlags srcAccess = fromShaderRead
        ? VK_ACCESS_SHADER_READ_BIT
        : 0;

    const VkImageMemoryBarrier barrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layerCount,
        },
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void ShadowMapArray::transition_to_shader_read(VkCommandBuffer cmd)
{
    const VkImageMemoryBarrier barrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layerCount,
        },
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void ShadowMapArray::destroy()
{
    auto& ctx = graphics::internal::context;
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(ctx.device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
    for (VkImageView view : layerImageViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(ctx.device, view, nullptr);
        }
    }
    layerImageViews.clear();
    if (arrayImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(ctx.device, arrayImageView, nullptr);
        arrayImageView = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(ctx.allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

} // namespace graphics::core
