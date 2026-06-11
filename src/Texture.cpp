#include "Texture.h"

#include "RenderUtils.h"
#include "Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>

// ----------------------------------------------------------------------------
// Image Utils
// ----------------------------------------------------------------------------

VkImageView ImageUtils::CreateImageView(
    const RenderContext & context,
    VkImage               image,
    VkFormat              format,
    VkImageAspectFlags    aspectFlags
)
{
    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView;

    if (vkCreateImageView(context.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("failed to create image view!");

    return imageView;
}

// ----------------------------------------------------------------------------
// Image
// ----------------------------------------------------------------------------

Image::Image(const RenderContext & context)
    : m_context(context)
    , m_image(VK_NULL_HANDLE)
    , m_imageView(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{
    // ...
}

Image::Image(Image && other) noexcept
    : m_context(other.m_context)
    , m_image(other.m_image)
    , m_imageView(other.m_imageView)
    , m_allocation(other.m_allocation)
{
    other.m_image      = VK_NULL_HANDLE;
    other.m_imageView  = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

Image & Image::operator=(Image && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_image      = other.m_image;
        m_imageView  = other.m_imageView;
        m_allocation = other.m_allocation;

        other.m_image      = VK_NULL_HANDLE;
        other.m_imageView  = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
    }

    return *this;
}

Image::~Image()
{
    Cleanup();
}

void Image::Create(
    uint32_t              width,
    uint32_t              height,
    VkFormat              format,
    VkImageTiling         tiling,
    VkImageUsageFlags     usage,
    VkMemoryPropertyFlags properties,
    VkImageAspectFlags    aspectFlags
)
{
    VkImageCreateInfo imageInfo {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    if (vmaCreateImage(m_context.allocator, &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr) != VK_SUCCESS)
        throw std::runtime_error("failed to create image!");

    m_imageView = ImageUtils::CreateImageView(m_context, m_image, format, aspectFlags);
}

void Image::CopyBuffer(VkBuffer buffer, uint32_t width, uint32_t height)
{
    TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(buffer, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Image::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = RenderUtils::BeginSingleTimeCommands(m_context);

    RenderUtils::AddImageMemoryBarrier(commandBuffer, m_image, oldLayout, newLayout);
    RenderUtils::EndSingleTimeCommands(m_context, commandBuffer);
}

void Image::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = RenderUtils::BeginSingleTimeCommands(m_context);

    VkBufferImageCopy region {};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    RenderUtils::EndSingleTimeCommands(m_context, commandBuffer);
}

void Image::Cleanup()
{
    if (m_imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_context.device, m_imageView, nullptr);
        vmaDestroyImage(m_context.allocator, m_image, m_allocation);

        m_image      = VK_NULL_HANDLE;
        m_imageView  = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

// ----------------------------------------------------------------------------
// Texture
// ----------------------------------------------------------------------------

Texture::Texture(const RenderContext & context, const std::string & filePath)
    : m_context(context)
    , m_image(context)
    , m_sampler(VK_NULL_HANDLE)
    , m_descriptorSet(VK_NULL_HANDLE)
{
    LoadFromFile(filePath);
    CreateSampler();
    CreateDescriptorSet();
}

Texture::Texture(Texture && other) noexcept
    : m_context(other.m_context)
    , m_image(std::move(other.m_image))
    , m_sampler(other.m_sampler)
    , m_descriptorSet(other.m_descriptorSet)
{
    other.m_sampler       = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;
}

Texture & Texture::operator=(Texture && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_image         = std::move(other.m_image);
        m_sampler       = other.m_sampler;
        m_descriptorSet = other.m_descriptorSet;

        other.m_sampler       = VK_NULL_HANDLE;
        other.m_descriptorSet = VK_NULL_HANDLE;
    }

    return *this;
}

Texture::~Texture()
{
    Cleanup();
}

void Texture::Bind(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
}

void Texture::LoadFromFile(const std::string & filepath)
{
    int texWidth;
    int texHeight;
    int texChannels;

    stbi_uc *    pixels    = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
        throw std::runtime_error("failed to load texture image!");

    Buffer stagingBuffer(m_context);

    stagingBuffer.Create(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.CopyData(pixels, imageSize);

    stbi_image_free(pixels);

    m_image.Create(
        texWidth,
        texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    m_image.CopyBuffer(stagingBuffer.GetBuffer(), texWidth, texHeight);
}

void Texture::Cleanup()
{
    if (m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_context.device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}

void Texture::CreateSampler()
{
    VkPhysicalDeviceProperties properties {};
    vkGetPhysicalDeviceProperties(m_context.physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_context.device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture sampler!");
}

void Texture::CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_context.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_context.textureLayout;

    if (vkAllocateDescriptorSets(m_context.device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets!");

    VkDescriptorImageInfo imageInfo {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = GetImageView();
    imageInfo.sampler     = GetSampler();

    VkWriteDescriptorSet descriptorWrite {};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = m_descriptorSet;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(m_context.device, 1, &descriptorWrite, 0, nullptr);
}
