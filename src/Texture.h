#pragma once

#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct RenderContext;

class ImageUtils
{
public:
    static VkImageView
    CreateImageView(const RenderContext & context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};

class Image
{
public:
    Image(const RenderContext & context);

    Image(const Image &)             = delete;
    Image & operator=(const Image &) = delete;

    Image(Image && other) noexcept;
    Image & operator=(Image && other) noexcept;

    ~Image();

public:
    void Create(
        uint32_t              width,
        uint32_t              height,
        VkFormat              format,
        VkImageTiling         tiling,
        VkImageUsageFlags     usage,
        VkMemoryPropertyFlags properties,
        VkImageAspectFlags    aspectFlags
    );

    void CopyBuffer(VkBuffer buffer, uint32_t width, uint32_t height);

    VkImageView GetView() const
    {
        return m_imageView;
    }

private:
    void Cleanup();

    void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);

private:
    const RenderContext & m_context;

    VkImage       m_image;
    VkImageView   m_imageView;
    VmaAllocation m_allocation;
};

class Texture
{
public:
    Texture(const RenderContext & context, const std::string & filePath);

    Texture(const Texture &)             = delete;
    Texture & operator=(const Texture &) = delete;

    Texture(Texture && other) noexcept;
    Texture & operator=(Texture && other) noexcept;

    ~Texture();

public:
    void Bind(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const;

    VkImageView GetImageView() const
    {
        return m_image.GetView();
    }

    VkSampler GetSampler() const
    {
        return m_sampler;
    }

private:
    void LoadFromFile(const std::string & filepath);
    void CreateSampler();
    void CreateDescriptorSet();
    void Cleanup();

private:
    const RenderContext & m_context;

    Image           m_image;
    VkSampler       m_sampler;
    VkDescriptorSet m_descriptorSet;
};