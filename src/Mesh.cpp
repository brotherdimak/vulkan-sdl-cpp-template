#include "Mesh.h"
#include "Renderer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <array>
#include <stdexcept>

// ----------------------------------------------------------------------------
// Vertex
// ----------------------------------------------------------------------------

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription {};

    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {};

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = offsetof(Vertex, pos);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset   = offsetof(Vertex, color);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

// ----------------------------------------------------------------------------
// Mesh
// ----------------------------------------------------------------------------

Mesh::Mesh(const RenderContext & context, const std::string & filePath)
    : m_context(context)
    , m_vertexBuffer(context)
    , m_indexBuffer(context)
{
    LoadFromFile(filePath);
    CreateVertexBuffer();
    CreateIndexBuffer();
};

Mesh::Mesh(Mesh && other) noexcept
    : m_context(other.m_context)
    , m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , m_vertexBuffer(std::move(other.m_vertexBuffer))
    , m_indexBuffer(std::move(other.m_indexBuffer))
{
    // ...
}

Mesh & Mesh::operator=(Mesh && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_vertices = std::move(other.m_vertices);
        m_indices  = std::move(other.m_indices);

        m_vertexBuffer = std::move(other.m_vertexBuffer);
        m_indexBuffer  = std::move(other.m_indexBuffer);
    }

    return *this;
}

Mesh::~Mesh()
{
    Cleanup();
}

void Mesh::LoadFromFile(const std::string & modelPath)
{
    Assimp::Importer importer;
    const aiScene *  scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));

    // Clear existing data
    m_vertices.clear();
    m_indices.clear();

    // Process all meshes in the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh * mesh        = scene->mMeshes[i];
        uint32_t indexOffset = static_cast<uint32_t>(m_vertices.size());

        // Process vertices
        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            Vertex vertex {};

            // Position
            vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            // Color (default to white if no vertex colors)
            if (mesh->mColors[0])
            {
                vertex.color = glm::vec3(mesh->mColors[0][j].r, mesh->mColors[0][j].g, mesh->mColors[0][j].b);
            }
            else
            {
                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
            }

            // Texture coordinates
            if (mesh->mTextureCoords[0])
            {
                vertex.texCoord = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            }
            else
            {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
            }

            m_vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                m_indices.push_back(static_cast<uint16_t>(indexOffset + face.mIndices[k]));
        }
    }

    SDL_Log("Loaded model: %s (%zu vertices, %zu indices)", modelPath.c_str(), m_vertices.size(), m_indices.size());
}

void Mesh::CreateVertexBuffer()
{
    m_vertexBuffer.CreateFromData(
        m_vertices.data(),
        sizeof(m_vertices[0]) * m_vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

void Mesh::CreateIndexBuffer()
{
    m_indexBuffer.CreateFromData(
        m_indices.data(),
        sizeof(m_indices[0]) * m_indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

void Mesh::Cleanup()
{
    m_vertices.clear();
    m_indices.clear();
}

void Mesh::Draw(VkCommandBuffer commandBuffer)
{
    VkBuffer     vertexBuffers[] = {m_vertexBuffer.GetBuffer()};
    VkDeviceSize offsets[]       = {0};

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
}