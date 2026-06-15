#include "ui/ViewportPanel.h"

#include <imgui.h>

#include <cstdint>

namespace lol
{
void ViewportPanel::Draw(AppState& state, Renderer& renderer)
{
    ImGui::Begin("Viewport");

    if (ImGui::Button("Reset Camera"))
    {
        state.camera.Reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus Model") && state.selection.model)
    {
        state.camera.FocusBounds(state.selection.model->boundsMin, state.selection.model->boundsMax);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &state.viewportOptions.showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &state.viewportOptions.wireframe);
    ImGui::SameLine();
    ImGui::Checkbox("Cull", &state.viewportOptions.backfaceCulling);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = static_cast<int>(available.x);
    const int height = static_cast<int>(available.y);
    renderer.Resize(width, height);
    state.camera.OnViewportResize(static_cast<float>(width), static_cast<float>(height));

    state.viewportHovered = ImGui::IsWindowHovered();
    state.viewportFocused = ImGui::IsWindowFocused();

    ImGui::Image(
        (ImTextureID)(intptr_t)renderer.OutputTexture(),
        available,
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    ImGui::End();
}
}
