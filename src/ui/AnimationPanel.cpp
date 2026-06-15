#include "ui/AnimationPanel.h"

#include <imgui.h>

#include <algorithm>

namespace lol
{
void AnimationPanel::Draw(AppState& state)
{
    ImGui::Begin("Animation");

    if (!state.selection.model)
    {
        ImGui::TextUnformatted("Open a model to inspect skeletons and animations.");
        ImGui::End();
        return;
    }

    const ModelData& model = *state.selection.model;
    if (model.skeleton.Empty())
    {
        ImGui::TextUnformatted("This model does not expose a skeleton through Assimp.");
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Show Skeleton", &state.viewportOptions.showSkeleton);
    ImGui::SameLine();
    if (ImGui::Checkbox("Pause##Checkbox", &state.pauseAnimation))
    {
        state.animator.SetPlaying(!state.pauseAnimation);
    }
    ImGui::Checkbox("Enable Skinning", &state.enableSkinning);

    if (!model.animationClips.empty())
    {
        int currentClip = state.animator.ActiveClipIndex();
        const bool validClipIndex = currentClip >= 0 && currentClip < static_cast<int>(model.animationClips.size());
        const char* preview = validClipIndex ? model.animationClips[currentClip].name.c_str() : "None";
        if (ImGui::BeginCombo("Clip", preview))
        {
            for (int index = 0; index < static_cast<int>(model.animationClips.size()); ++index)
            {
                const bool selected = currentClip == index;
                if (ImGui::Selectable(model.animationClips[index].name.c_str(), selected))
                {
                    state.animator.SetActiveClip(index);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button(state.pauseAnimation ? "Play##Playback" : "Pause##Playback"))
        {
            state.pauseAnimation = !state.pauseAnimation;
            state.animator.SetPlaying(!state.pauseAnimation);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step -"))
        {
            state.pauseAnimation = true;
            state.animator.SetPlaying(false);
            state.animator.Step(-1.0f / 30.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step +"))
        {
            state.pauseAnimation = true;
            state.animator.SetPlaying(false);
            state.animator.Step(1.0f / 30.0f);
        }

        float speed = state.animator.PlaybackSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 3.0f, "%.2fx"))
        {
            state.animator.SetPlaybackSpeed(speed);
        }

        bool loop = state.animator.Looping();
        if (ImGui::Checkbox("Loop", &loop))
        {
            state.animator.SetLooping(loop);
        }

        if (const AnimationClip* clip = state.animator.ActiveClip())
        {
            float currentTime = state.animator.CurrentTime();
            if (ImGui::SliderFloat("Time", &currentTime, 0.0f, std::max(clip->durationSeconds, 0.001f), "%.3fs"))
            {
                state.pauseAnimation = true;
                state.animator.SetPlaying(false);
                state.animator.SetCurrentTime(currentTime);
            }
            ImGui::Text("Duration: %.3fs", clip->durationSeconds);
            ImGui::Text("Tracks: %d", static_cast<int>(clip->tracks.size()));
        }

        if (!state.animator.ValidationMessage().empty())
        {
            ImGui::Separator();
            ImGui::TextWrapped("Playback warning: %s", state.animator.ValidationMessage().c_str());
        }
    }
    else
    {
        ImGui::TextUnformatted("Skeleton found, but no animation clips were imported.");
    }

    ImGui::Separator();
    ImGui::Text("Bones: %d", static_cast<int>(model.skeleton.bones.size()));
    ImGui::End();
}
}
