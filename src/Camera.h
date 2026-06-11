#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    Camera();

public:
    void HandleInput(float deltaTime);

    void UpdateProjection();
    void UpdateView();

    void SetFov(float fov);
    void SetScreenSize(int width, int height);
    void SetNearFarPlanes(float near, float far);

    void SetPosition(const glm::vec3 & position);
    void SetPitch(float pitch);
    void SetYaw(float yaw);

    void MoveForward(float speed);
    void MoveBack(float speed);
    void MoveLeft(float speed);
    void MoveRight(float speed);

    void RotatePitch(float speed);
    void RotateYaw(float speed);

    const glm::mat4 & GetView() const
    {
        return m_view;
    }
    const glm::mat4 & GetProj() const
    {
        return m_proj;
    }

private:
    float m_fov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    bool m_isDirty;

    float m_pitch;
    float m_yaw;
    float m_roll;

    glm::vec3 m_position;
    glm::vec3 m_forward;
    glm::vec3 m_up;
    glm::vec3 m_right;

    glm::mat4 m_view;
    glm::mat4 m_proj;
};