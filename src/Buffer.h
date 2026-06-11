#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct RenderContext;

class Buffer
{
public:
    Buffer(const RenderContext & context);

    Buffer(const Buffer &)             = delete;
    Buffer & operator=(const Buffer &) = delete;

    Buffer(Buffer && other) noexcept;
    Buffer & operator=(Buffer && other) noexcept;

    ~Buffer();

public:
    void Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void CreateFromData(const void * data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void CopyData(const void * srcData, VkDeviceSize size);
    void CopyBuffer(VkBuffer srcBuffer, VkDeviceSize size);

    VkBuffer GetBuffer() const
    {
        return m_buffer;
    }
    VmaAllocation GetAllocation() const
    {
        return m_allocation;
    }

private:
    void Cleanup();

private:
    const RenderContext & m_context;

    VkBuffer      m_buffer;
    VmaAllocation m_allocation;
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class UniformBuffer
{
public:
    UniformBuffer(const RenderContext & context);

    UniformBuffer(const UniformBuffer &)             = delete;
    UniformBuffer & operator=(const UniformBuffer &) = delete;

    UniformBuffer(UniformBuffer && other) noexcept;
    UniformBuffer & operator=(UniformBuffer && other) noexcept;

    ~UniformBuffer();

public:
    void Update(uint32_t currentFrame, const UniformBufferObject & ubo);

    VkBuffer GetBuffer(size_t frame) const
    {
        return m_buffers[frame].GetBuffer();
    }

private:
    void Create();
    void Cleanup();

private:
    const RenderContext & m_context;

    std::vector<Buffer> m_buffers;
    std::vector<void *> m_mappedBuffers;
};