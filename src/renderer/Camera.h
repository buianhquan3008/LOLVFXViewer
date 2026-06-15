#pragma once

#include <glm/glm.hpp>

namespace lol
{
class OrbitCamera
{
public:
    void Update(float deltaSeconds, bool hovered, bool focused);
    void OnViewportResize(float width, float height);
    void Reset();
    void FocusBounds(const glm::vec3& boundsMin, const glm::vec3& boundsMax);

    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjectionMatrix() const;
    glm::vec3 Position() const;

private:
    float m_distance = 6.0f;
    float m_yaw = 0.75f;
    float m_pitch = 0.45f;
    glm::vec3 m_target = glm::vec3(0.0f);
    glm::vec2 m_viewportSize = glm::vec2(1280.0f, 720.0f);
};
}
