#pragma once

#include <string>
#include <memory>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan_core.h>

#include "Mesh.h"
#include "Texture.h"

struct RenderContext;

class SceneObject
{
public:
    struct PushConstants
    {
        alignas(16) glm::mat4 model;
    };

public:
    SceneObject(
        const RenderContext & context,
        const std::string &   name,
        const std::string &   texturePath,
        const std::string &   meshPath,
        const glm::vec3 &     position,
        const glm::vec3 &     rotation,
        float                 scale
    );

    SceneObject(const SceneObject & other)             = delete;
    SceneObject & operator=(const SceneObject & other) = delete;

    SceneObject(SceneObject && other) noexcept             = default;
    SceneObject & operator=(SceneObject && other) noexcept = default;

    ~SceneObject() = default;

public:
    void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    const std::string & GetName()
    {
        return m_name;
    }

    bool GetVisible() const
    {
        return m_visible;
    }

    void SetVisible(bool visible)
    {
        m_visible = visible;
    }

private:
    std::string              m_name;
    std::unique_ptr<Texture> m_texture;
    std::unique_ptr<Mesh>    m_mesh;
    glm::vec3                m_position;
    glm::vec3                m_rotation;
    float                    m_scale;
    bool                     m_visible;
};