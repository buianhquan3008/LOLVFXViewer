#include "ui/AssetBrowserPanel.h"

#include "core/Log.h"

#include <imgui.h>
#include <cstdio>

namespace lol
{
void AssetBrowserPanel::Draw(AppState& state)
{
    ImGui::Begin("Asset Browser");

    if (state.rootPathInput.empty() && !state.assetManager.Root().empty())
    {
        state.rootPathInput = state.assetManager.Root().string();
    }

    char rootBuffer[512]{};
    std::snprintf(rootBuffer, sizeof(rootBuffer), "%s", state.rootPathInput.c_str());
    if (ImGui::InputText("Root Folder", rootBuffer, sizeof(rootBuffer)))
    {
        state.rootPathInput = rootBuffer;
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Root"))
    {
        state.assetManager.SetRoot(state.rootPathInput);
        state.currentDirectory = state.rootPathInput;
        Log::Push(LogLevel::Info, "Asset root changed", state.currentDirectory.string());
    }

    char searchBuffer[128]{};
    std::snprintf(searchBuffer, sizeof(searchBuffer), "%s", state.searchText.c_str());
    if (ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer)))
    {
        state.searchText = searchBuffer;
    }

    ImGui::Separator();

    if (!state.assetManager.Root().empty() && std::filesystem::exists(state.assetManager.Root()))
    {
        ImGui::BeginChild("DirectoryTree", ImVec2(250.0f, 0.0f), true);
        DrawDirectoryTree(state, state.assetManager.Root());
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("DirectoryContents", ImVec2(0.0f, 0.0f), true);
        ImGui::TextUnformatted(state.currentDirectory.string().c_str());
        ImGui::Separator();

        for (const auto& entry : state.assetManager.EnumerateDirectory(state.currentDirectory))
        {
            const std::string name = entry.path().filename().string();
            if (!state.searchText.empty() && name.find(state.searchText) == std::string::npos)
            {
                continue;
            }

            if (entry.is_directory())
            {
                if (ImGui::Selectable((name + "/").c_str(), false))
                {
                    state.currentDirectory = entry.path();
                }
            }
            else
            {
                const bool supported =
                    state.assetManager.IsSupportedTexture(entry.path()) ||
                    state.assetManager.IsSupportedModel(entry.path()) ||
                    state.assetManager.IsSupportedVfx(entry.path()) ||
                    state.assetManager.IsDataFile(entry.path());
                const std::string label = supported ? name : (name + " (meta)");
                if (ImGui::Selectable(label.c_str(), state.selection.path == entry.path()))
                {
                    state.selection = state.assetManager.Open(entry.path());
                    Log::Push(LogLevel::Info, "Opened asset", entry.path().string());
                }
            }
        }
        ImGui::EndChild();
    }
    else
    {
        ImGui::TextWrapped("Enter an extracted asset folder path and click Load Root.");
    }

    ImGui::End();
}

void AssetBrowserPanel::DrawDirectoryTree(AppState& state, const std::filesystem::path& path)
{
    const std::string label = path.filename().empty() ? path.string() : path.filename().string();
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
        ((state.currentDirectory == path) ? ImGuiTreeNodeFlags_Selected : 0);

    const bool open = ImGui::TreeNodeEx(path.string().c_str(), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked())
    {
        state.currentDirectory = path;
    }

    if (open)
    {
        for (const auto& entry : state.assetManager.EnumerateDirectory(path))
        {
            if (entry.is_directory())
            {
                DrawDirectoryTree(state, entry.path());
            }
        }
        ImGui::TreePop();
    }
}
}
