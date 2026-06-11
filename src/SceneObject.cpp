#include "SceneObject.h"
#include "Renderer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

SceneObject::SceneObject(
    const RenderContext & context,
    const std::string &   name,
    const std::string &   texturePath,
    const std::string &   meshPath,
    const glm::vec3 &     position,
    const glm::vec3 &     rotation,
    float                 scale
)
    : m_name(name)
    , m_texture(std::make_unique<Texture>(context, texturePath))
    , m_mesh(std::make_unique<Mesh>(context, meshPath))
    , m_position(position)
    , m_rotation(rotation)
    , m_scale(scale)
    , m_visible(true)
{
    // ...
}

void SceneObject::Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
    SceneObject::PushConstants constants {};

    constants.model = glm::mat4(1.0f);
    constants.model = glm::translate(constants.model, m_position);
    constants.model = glm::rotate(constants.model, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    constants.model = glm::rotate(constants.model, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    constants.model = glm::rotate(constants.model, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    constants.model = glm::scale(constants.model, glm::vec3(m_scale));

    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(SceneObject::PushConstants),
        &constants
    );

    m_texture->Bind(commandBuffer, pipelineLayout);

    m_mesh->Draw(commandBuffer);
}
