#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vulkan/vulkan_core.h>

#include "Buffer.h"

struct RenderContext;

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription                  GetBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};

class Mesh
{
public:
    Mesh(const RenderContext & context, const std::string & filePath);

    Mesh(const Mesh &)             = delete;
    Mesh & operator=(const Mesh &) = delete;

    Mesh(Mesh && other) noexcept;
    Mesh & operator=(Mesh && other) noexcept;

    ~Mesh();

public:
    void Draw(VkCommandBuffer commandBuffer);
    void Cleanup();

private:
    void LoadFromFile(const std::string & modelPath);

    void CreateVertexBuffer();
    void CreateIndexBuffer();

private:
    const RenderContext & m_context;

    std::vector<Vertex>   m_vertices;
    std::vector<uint16_t> m_indices;

    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
};