#include "platform/Window.h"

#include <stdexcept>

namespace lol
{
namespace
{
int g_windowRefCount = 0;
}

Window::Window(int width, int height, std::string title)
{
    if (g_windowRefCount == 0 && glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    ++g_windowRefCount;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (m_window == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
}

Window::~Window()
{
    if (m_window != nullptr)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    --g_windowRefCount;
    if (g_windowRefCount == 0)
    {
        glfwTerminate();
    }
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::SwapBuffers() const
{
    glfwSwapBuffers(m_window);
}

void Window::PollEvents() const
{
    glfwPollEvents();
}

void Window::GetFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(m_window, &width, &height);
}
}
