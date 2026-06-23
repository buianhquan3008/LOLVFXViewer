#include "ui/InspectorPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace lol
{
namespace
{
void DrawBoneTree(AppState& state, const Skeleton& skeleton, int boneIndex)
{
    const Bone& bone = skeleton.bones[boneIndex];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (state.selectedBoneIndex == boneIndex)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool hasChild = false;
    for (std::size_t childIndex = 0; childIndex < skeleton.bones.size(); ++childIndex)
    {
        if (skeleton.bones[childIndex].parentIndex == boneIndex)
        {
            hasChild = true;
            break;
        }
    }
    if (!hasChild)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    const bool open = ImGui::TreeNodeEx((void*)(intptr_t)boneIndex, flags, "%s", bone.name.c_str());
    if (ImGui::IsItemClicked())
    {
        state.selectedBoneIndex = boneIndex;
    }

    if (open)
    {
        for (std::size_t childIndex = 0; childIndex < skeleton.bones.size(); ++childIndex)
        {
            if (skeleton.bones[childIndex].parentIndex == boneIndex)
            {
                DrawBoneTree(state, skeleton, static_cast<int>(childIndex));
            }
        }
        ImGui::TreePop();
    }
}
}

void InspectorPanel::Draw(AppState& state)
{
    ImGui::Begin("Inspector");

    if (state.selection.type == AssetSelectionType::None)
    {
        ImGui::TextUnformatted("No asset selected.");
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("%s", state.selection.path.string().c_str());
    ImGui::Separator();

    switch (state.selection.type)
    {
    case AssetSelectionType::Texture:
        if (state.selection.texture)
        {
            const TextureInfo& info = state.selection.texture->Info();
            ImGui::Text("Type: Texture");
            ImGui::Text("Size: %d x %d", info.width, info.height);
            ImGui::Text("Channels: %d", info.channels);
            ImGui::Text("OpenGL ID: %u", info.rendererId);
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = width * (static_cast<float>(info.height) / static_cast<float>(std::max(info.width, 1)));
            ImGui::Image((ImTextureID)(intptr_t)info.rendererId, ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
        }
        break;
    case AssetSelectionType::Model:
        if (state.selection.model)
        {
            std::size_t vertexCount = 0;
            std::size_t indexCount = 0;
            std::size_t weightedVertexCount = 0;
            std::size_t animatedWeightedBoneCount = 0;
            float maxWeightedBoneMovement = 0.0f;
            int maxInfluenceCount = 0;
            for (const auto& mesh : state.selection.model->meshes)
            {
                vertexCount += mesh.vertices.size();
                indexCount += mesh.indices.size();
                for (const auto& vertex : mesh.vertices)
                {
                    int influenceCount = 0;
                    for (int slot = 0; slot < 4; ++slot)
                    {
                        if (vertex.boneWeights[slot] > 0.0f)
                        {
                            ++influenceCount;
                        }
                    }
                    if (influenceCount > 0)
                    {
                        ++weightedVertexCount;
                    }
                    maxInfluenceCount = std::max(maxInfluenceCount, influenceCount);
                }
            }

            if (!state.selection.model->animationClips.empty())
            {
                const auto& activeTracks = state.selection.model->animationClips.front().tracks;
                for (int boneIndex : state.selection.model->weightedBoneIndices)
                {
                    if (boneIndex >= 0 && boneIndex < static_cast<int>(state.selection.model->skeleton.bones.size()))
                    {
                        const std::string& boneName = state.selection.model->skeleton.bones[boneIndex].name;
                        if (activeTracks.contains(boneName))
                        {
                            ++animatedWeightedBoneCount;
                        }
                        maxWeightedBoneMovement = std::max(maxWeightedBoneMovement, state.animator.BoneMovementDistance(boneIndex));
                    }
                }
            }

            ImGui::Text("Type: Model");
            ImGui::Text("Meshes: %d", static_cast<int>(state.selection.model->meshes.size()));
            ImGui::Text("Vertices: %d", static_cast<int>(vertexCount));
            ImGui::Text("Indices: %d", static_cast<int>(indexCount));
            ImGui::Text("Weighted Vertices: %d", static_cast<int>(weightedVertexCount));
            ImGui::Text("Max Influences Per Vertex: %d", maxInfluenceCount);
            ImGui::Text("Weighted Bones: %d", static_cast<int>(state.selection.model->weightedBoneIndices.size()));
            ImGui::Text("Animated Weighted Bones: %d", static_cast<int>(animatedWeightedBoneCount));
            ImGui::Text("Max Weighted Bone Movement: %.3f", maxWeightedBoneMovement);
            ImGui::Text("Bounds Min: %.2f %.2f %.2f", state.selection.model->boundsMin.x, state.selection.model->boundsMin.y, state.selection.model->boundsMin.z);
            ImGui::Text("Bounds Max: %.2f %.2f %.2f", state.selection.model->boundsMax.x, state.selection.model->boundsMax.y, state.selection.model->boundsMax.z);
            ImGui::Text("Bones: %d", static_cast<int>(state.selection.model->skeleton.bones.size()));
            ImGui::Text("Animation Clips: %d", static_cast<int>(state.selection.model->animationClips.size()));

            if (!state.selection.model->skeleton.Empty())
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Bone Hierarchy");
                ImGui::BeginChild("BoneTree", ImVec2(0.0f, 180.0f), true);
                for (std::size_t boneIndex = 0; boneIndex < state.selection.model->skeleton.bones.size(); ++boneIndex)
                {
                    if (state.selection.model->skeleton.bones[boneIndex].parentIndex < 0)
                    {
                        DrawBoneTree(state, state.selection.model->skeleton, static_cast<int>(boneIndex));
                    }
                }
                ImGui::EndChild();

                if (state.selectedBoneIndex >= 0 && state.selectedBoneIndex < static_cast<int>(state.selection.model->skeleton.bones.size()))
                {
                    const Bone& bone = state.selection.model->skeleton.bones[state.selectedBoneIndex];
                    ImGui::Separator();
                    ImGui::Text("Selected Bone: %s", bone.name.c_str());
                    ImGui::Text("Bind Position: %.2f %.2f %.2f", bone.bindPosition.x, bone.bindPosition.y, bone.bindPosition.z);
                    ImGui::Text("Bind Scale: %.2f %.2f %.2f", bone.bindScale.x, bone.bindScale.y, bone.bindScale.z);
                    if (state.selectedBoneIndex < static_cast<int>(state.animator.GlobalPose().size()))
                    {
                        const glm::vec3 animatedPosition = glm::vec3(state.animator.GlobalPose()[state.selectedBoneIndex][3]);
                        ImGui::Text("Animated Position: %.2f %.2f %.2f", animatedPosition.x, animatedPosition.y, animatedPosition.z);
                    }
                }
            }
        }
        break;
    case AssetSelectionType::Vfx:
        if (state.selection.vfx)
        {
            ImGui::TextUnformatted("Type: VFX");
            ImGui::Text("Graph: %s", state.selection.vfx->name.c_str());
            ImGui::Text("Emitters: %d", static_cast<int>(state.selection.vfx->emitters.size()));
            ImGui::Text("Materials: %d", static_cast<int>(state.selection.vfx->materials.size()));
            ImGui::Text("Children: %d", static_cast<int>(state.selection.vfx->children.size()));
            if (!state.selection.metadata.empty())
            {
                ImGui::Separator();
                ImGui::TextWrapped("%s", state.selection.metadata.c_str());
            }
        }
        break;
    case AssetSelectionType::Data:
        ImGui::TextUnformatted("Type: Data");
        ImGui::Separator();
        ImGui::BeginChild("DataPreview", ImVec2(0.0f, 250.0f), true);
        ImGui::TextWrapped("%s", state.selection.metadata.c_str());
        ImGui::EndChild();
        break;
    case AssetSelectionType::Unsupported:
        ImGui::TextUnformatted("Type: Unsupported");
        ImGui::TextWrapped("%s", state.selection.metadata.c_str());
        break;
    case AssetSelectionType::None:
        break;
    }

    ImGui::End();
}
}
