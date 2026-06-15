#pragma once

#include "core/Time.h"
#include "platform/Window.h"
#include "renderer/Renderer.h"
#include "ui/AppState.h"
#include "ui/AssetBrowserPanel.h"
#include "ui/AnimationPanel.h"
#include "ui/ConsolePanel.h"
#include "ui/InspectorPanel.h"
#include "ui/ViewportPanel.h"

#include <filesystem>
#include <memory>

namespace lol
{
class Application
{
public:
    Application();
    ~Application();

    void Run();

private:
    void BeginFrame();
    void DrawDockspace();
    void EndFrame();
    void SyncRendererSelection();
    void ValidateOpenGL();

    Window m_window;
    std::unique_ptr<Renderer> m_renderer;
    AppState m_state;
    AssetBrowserPanel m_assetBrowserPanel;
    AnimationPanel m_animationPanel;
    InspectorPanel m_inspectorPanel;
    ConsolePanel m_consolePanel;
    ViewportPanel m_viewportPanel;
    TimeStep m_timeStep;
    std::filesystem::path m_lastRenderedModelPath;
};
}
