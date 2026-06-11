#include "Buffer.h"

#include "RenderUtils.h"
#include "Renderer.h"

#include <stdexcept>

// ----------------------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------------------

Buffer::Buffer(const RenderContext & context)
    : m_context(context)
    , m_buffer(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{
    // ...
};

Buffer::Buffer(Buffer && other) noexcept
    : m_context(other.m_context)
    , m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
{
    other.m_buffer     = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

Buffer & Buffer::operator=(Buffer && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_buffer     = other.m_buffer;
        m_allocation = other.m_allocation;

        other.m_buffer     = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
    }

    return *this;
}

Buffer::~Buffer()
{
    Cleanup();
}

void Buffer::Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    else
        allocInfo.flags = 0;

    if (vmaCreateBuffer(m_context.allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS)
        throw std::runtime_error("failed to create buffer!");
}

void Buffer::CreateFromData(
    const void *          data,
    VkDeviceSize          size,
    VkBufferUsageFlags    usage,
    VkMemoryPropertyFlags properties
)
{
    Buffer stagingBuffer(m_context);

    stagingBuffer.Create(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.CopyData(data, size);

    Create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, properties);

    CopyBuffer(stagingBuffer.GetBuffer(), size);
}

void Buffer::Cleanup()
{
    if (m_buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(m_context.allocator, m_buffer, m_allocation);

        m_buffer     = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

void Buffer::CopyData(const void * srcData, VkDeviceSize size)
{
    void * data;

    vmaMapMemory(m_context.allocator, m_allocation, &data);
    memcpy(data, srcData, (size_t)size);
    vmaUnmapMemory(m_context.allocator, m_allocation);
}

void Buffer::CopyBuffer(VkBuffer srcBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = RenderUtils::BeginSingleTimeCommands(m_context);

    VkBufferCopy copyRegion {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, m_buffer, 1, &copyRegion);

    RenderUtils::EndSingleTimeCommands(m_context, commandBuffer);
}

// ----------------------------------------------------------------------------
// Uniform Buffer
// ----------------------------------------------------------------------------

UniformBuffer::UniformBuffer(const RenderContext & context)
    : m_context(context)
{
    Create();
}

UniformBuffer::UniformBuffer(UniformBuffer && other) noexcept
    : m_context(other.m_context)
    , m_buffers(std::move(other.m_buffers))
    , m_mappedBuffers(std::move(other.m_mappedBuffers))
{
    // ...
}

UniformBuffer & UniformBuffer::operator=(UniformBuffer && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_buffers       = std::move(other.m_buffers);
        m_mappedBuffers = std::move(other.m_mappedBuffers);
    }

    return *this;
}

UniformBuffer::~UniformBuffer()
{
    Cleanup();
}

void UniformBuffer::Create()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_buffers.reserve(m_context.maxFrames);
    m_mappedBuffers.resize(m_context.maxFrames);

    for (size_t i = 0; i < m_context.maxFrames; i++)
    {
        m_buffers.emplace_back(m_context);

        m_buffers[i].Create(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        vmaMapMemory(m_context.allocator, m_buffers[i].GetAllocation(), &m_mappedBuffers[i]);
    }
}

void UniformBuffer::Update(uint32_t currentFrame, const UniformBufferObject & ubo)
{
    memcpy(m_mappedBuffers[currentFrame], &ubo, sizeof(ubo));
}

void UniformBuffer::Cleanup()
{
    for (size_t i = 0; i < m_buffers.size(); i++)
    {
        if (m_mappedBuffers[i] != nullptr)
        {
            vmaUnmapMemory(m_context.allocator, m_buffers[i].GetAllocation());
            m_mappedBuffers[i] = nullptr;
        }
    }

    m_buffers.clear();
    m_mappedBuffers.clear();
}