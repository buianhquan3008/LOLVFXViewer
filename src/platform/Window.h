#pragma once

#include <GLFW/glfw3.h>
#include <string>

namespace lol
{
class Window
{
public:
    Window(int width, int height, std::string title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* NativeHandle() const { return m_window; }
    bool ShouldClose() const;
    void SwapBuffers() const;
    void PollEvents() const;
    void GetFramebufferSize(int& width, int& height) const;

private:
    GLFWwindow* m_window = nullptr;
};
}
