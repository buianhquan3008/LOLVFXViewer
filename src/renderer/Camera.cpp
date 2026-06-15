#include "renderer/Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace lol
{
void OrbitCamera::Update(float deltaSeconds, bool hovered, bool focused)
{
    if (!hovered || !focused)
    {
        return;
    }

    GLFWwindow* context = glfwGetCurrentContext();
    if (context == nullptr)
    {
        return;
    }

    const float rotateSpeed = 1.75f;
    const float panSpeed = 4.0f * deltaSeconds * m_distance;
    const float zoomSpeed = 12.0f * deltaSeconds;

    if (glfwGetMouseButton(context, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        static double lastX = 0.0;
        static double lastY = 0.0;
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(context, &x, &y);
        const float dx = static_cast<float>(x - lastX);
        const float dy = static_cast<float>(y - lastY);
        m_yaw -= dx * rotateSpeed * deltaSeconds;
        m_pitch -= dy * rotateSpeed * deltaSeconds;
        m_pitch = std::clamp(m_pitch, -1.4f, 1.4f);
        lastX = x;
        lastY = y;
    }

    if (glfwGetMouseButton(context, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
    {
        const glm::vec3 forward = glm::normalize(m_target - Position());
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        if (glfwGetKey(context, GLFW_KEY_A) == GLFW_PRESS) m_target -= right * panSpeed;
        if (glfwGetKey(context, GLFW_KEY_D) == GLFW_PRESS) m_target += right * panSpeed;
        if (glfwGetKey(context, GLFW_KEY_W) == GLFW_PRESS) m_target += up * panSpeed;
        if (glfwGetKey(context, GLFW_KEY_S) == GLFW_PRESS) m_target -= up * panSpeed;
    }

    if (glfwGetKey(context, GLFW_KEY_Q) == GLFW_PRESS) m_distance += zoomSpeed;
    if (glfwGetKey(context, GLFW_KEY_E) == GLFW_PRESS) m_distance -= zoomSpeed;
    m_distance = std::max(m_distance, 0.5f);
}

void OrbitCamera::OnViewportResize(float width, float height)
{
    m_viewportSize = glm::vec2(std::max(width, 1.0f), std::max(height, 1.0f));
}

void OrbitCamera::Reset()
{
    m_distance = 6.0f;
    m_yaw = 0.75f;
    m_pitch = 0.45f;
    m_target = glm::vec3(0.0f);
}

void OrbitCamera::FocusBounds(const glm::vec3& boundsMin, const glm::vec3& boundsMax)
{
    m_target = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 extent = boundsMax - boundsMin;
    const float radius = std::max(std::max(extent.x, extent.y), extent.z) * 0.6f;
    m_distance = std::max(radius * 2.25f, 2.0f);
    m_yaw = 0.75f;
    m_pitch = 0.35f;
}

glm::mat4 OrbitCamera::ViewMatrix() const
{
    return glm::lookAt(Position(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 OrbitCamera::ProjectionMatrix() const
{
    return glm::perspective(glm::radians(50.0f), m_viewportSize.x / m_viewportSize.y, 0.01f, 1000.0f);
}

glm::vec3 OrbitCamera::Position() const
{
    return {
        m_target.x + m_distance * std::cos(m_pitch) * std::sin(m_yaw),
        m_target.y + m_distance * std::sin(m_pitch),
        m_target.z + m_distance * std::cos(m_pitch) * std::cos(m_yaw)
    };
}
}
