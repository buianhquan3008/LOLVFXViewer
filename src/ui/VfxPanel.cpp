#include "ui/VfxPanel.h"

#include <imgui.h>

namespace lol
{
void VfxPanel::Draw(AppState& state)
{
    ImGui::Begin("VFX");

    if (!state.selection.vfx)
    {
        ImGui::TextUnformatted("Open a VFX asset to inspect emitters and particle runtime.");
        ImGui::End();
        return;
    }

    const VfxGraph& graph = *state.selection.vfx;
    ImGui::Text("Graph: %s", graph.name.c_str());
    ImGui::Text("Emitters: %d", static_cast<int>(graph.emitters.size()));
    ImGui::Text("Materials: %d", static_cast<int>(graph.materials.size()));
    ImGui::Text("Children: %d", static_cast<int>(graph.children.size()));
    ImGui::Text("Particles Alive: %d", static_cast<int>(state.vfxRuntime.ParticleCount()));
    ImGui::Text("Elapsed: %.2fs", state.vfxRuntime.ElapsedSeconds());
    ImGui::Checkbox("Enable VFX", &state.enableVfx);
    ImGui::SameLine();
    ImGui::Checkbox("Pause VFX", &state.pauseVfx);
    ImGui::Checkbox("Show VFX", &state.viewportOptions.showVfx);

    if (ImGui::Button("Restart VFX"))
    {
        state.vfxRuntime.Restart();
    }

    if (!state.selection.metadata.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("Loader notes: %s", state.selection.metadata.c_str());
    }

    ImGui::Separator();
    for (std::size_t index = 0; index < graph.emitters.size(); ++index)
    {
        const VfxEmitter& emitter = graph.emitters[index];
        if (ImGui::TreeNode(reinterpret_cast<void*>(index + 1), "%s", emitter.name.c_str()))
        {
            ImGui::Text("Spawn Rate: %.2f", emitter.spawnRate);
            ImGui::Text("Lifetime: %.2f -> %.2f", emitter.lifetimeMin, emitter.lifetimeMax);
            ImGui::Text("Size: %.2f -> %.2f", emitter.startSize, emitter.endSize);
            ImGui::Text("Max Particles: %d", emitter.maxParticles);
            ImGui::Text("Texture: %s", emitter.texturePath.empty() ? "<default>" : emitter.texturePath.c_str());
            ImGui::Text("Material: %s", emitter.materialName.empty() ? "<none>" : emitter.materialName.c_str());
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
}
