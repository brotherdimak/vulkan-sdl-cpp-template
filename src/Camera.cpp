#include "Camera.h"

#include <SDL3/SDL.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{

constexpr glm::vec3 WORLD_UP(0.0f, 0.0f, 1.0f);

constexpr float PITCH_MIN = -89.0f;
constexpr float PITCH_MAX = 89.0f;

constexpr float MOVE_SPEED  = 12.0f;
constexpr float SENSITIVITY = 400.0f;

} // namespace

Camera::Camera()
    : m_fov(70.0f)
    , m_aspectRatio(1.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(100.0f)
    , m_isDirty(true)
    , m_pitch(0.0f)
    , m_yaw(0.0f)
    , m_roll(0.0f)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_forward(1.0f, 0.0f, 0.0f)
    , m_up(0.0f, 0.0f, 1.0f)
    , m_right(0.0f, 1.0f, 0.0f)
    , m_view(1.0f)
    , m_proj(1.0f)
{
    UpdateProjection();
}

void Camera::HandleInput(float deltaTime)
{
    int          numKeys;
    const bool * keyboardState = SDL_GetKeyboardState(&numKeys);

    if (keyboardState[SDL_SCANCODE_W])
        MoveForward(MOVE_SPEED * deltaTime);
    else if (keyboardState[SDL_SCANCODE_S])
        MoveBack(MOVE_SPEED * deltaTime);

    if (keyboardState[SDL_SCANCODE_A])
        MoveLeft(MOVE_SPEED * deltaTime);
    else if (keyboardState[SDL_SCANCODE_D])
        MoveRight(MOVE_SPEED * deltaTime);

    float      mouseRelX, mouseRelY;
    const auto mouseReState = SDL_GetRelativeMouseState(&mouseRelX, &mouseRelY);

    if (mouseReState & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
    {
        RotatePitch(-mouseRelY * SENSITIVITY * deltaTime);
        RotateYaw(-mouseRelX * SENSITIVITY * deltaTime);
    }

    UpdateView();
}

void Camera::UpdateProjection()
{
    m_proj = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::UpdateView()
{
    if (!m_isDirty)
        return;

    m_isDirty = false;

    glm::vec3 newForward {};
    newForward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    newForward.y = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    newForward.z = sin(glm::radians(m_pitch));

    m_forward = glm::normalize(newForward);
    m_right   = glm::normalize(glm::cross(m_forward, WORLD_UP));
    m_up      = glm::normalize(glm::cross(m_right, m_forward));

    m_view = glm::lookAt(m_position, m_position + m_forward, m_up);
}

void Camera::SetFov(float fov)
{
    m_fov = fov;
    UpdateProjection();
}

void Camera::SetScreenSize(int width, int height)
{
    m_aspectRatio = (height > 0) ? width / (float)height : 1.0f;
    UpdateProjection();
}

void Camera::SetNearFarPlanes(float near, float far)
{
    m_nearPlane = near;
    m_farPlane  = far;
    UpdateProjection();
}

void Camera::SetPosition(const glm::vec3 & position)
{
    m_position = position;
    m_isDirty  = true;
}

void Camera::SetPitch(float pitch)
{
    m_pitch   = glm::clamp(pitch, PITCH_MIN, PITCH_MAX);
    m_isDirty = true;
}

void Camera::SetYaw(float yaw)
{
    m_yaw     = yaw;
    m_isDirty = true;
}

void Camera::MoveForward(float speed)
{
    m_position += m_forward * speed;
    m_isDirty = true;
}

void Camera::MoveBack(float speed)
{
    m_position -= m_forward * speed;
    m_isDirty = true;
}

void Camera::MoveLeft(float speed)
{
    m_position -= m_right * speed;
    m_isDirty = true;
}

void Camera::MoveRight(float speed)
{
    m_position += m_right * speed;
    m_isDirty = true;
}

void Camera::RotatePitch(float speed)
{
    m_pitch   = glm::clamp(m_pitch + speed, PITCH_MIN, PITCH_MAX);
    m_isDirty = true;
}

void Camera::RotateYaw(float speed)
{
    m_yaw += speed;
    m_isDirty = true;
}