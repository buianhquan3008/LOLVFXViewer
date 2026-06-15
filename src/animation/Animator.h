#pragma once

#include "animation/AnimationClip.h"
#include "animation/Skeleton.h"

#include <glm/glm.hpp>

#include <vector>

namespace lol
{
class Animator
{
public:
    void Clear();
    void SetData(const Skeleton* skeleton, const std::vector<AnimationClip>* clips);
    void Update(float deltaSeconds);
    void SetActiveClip(int clipIndex);
    void SetCurrentTime(float timeSeconds);
    void Step(float deltaSeconds);

    bool HasSkeleton() const;
    bool HasClips() const;
    bool IsPlaying() const { return m_isPlaying; }
    void SetPlaying(bool isPlaying) { m_isPlaying = isPlaying; }
    bool Looping() const { return m_looping; }
    void SetLooping(bool looping) { m_looping = looping; }
    float PlaybackSpeed() const { return m_playbackSpeed; }
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float CurrentTime() const { return m_currentTimeSeconds; }
    int ActiveClipIndex() const { return m_activeClipIndex; }
    const AnimationClip* ActiveClip() const;
    const std::vector<glm::mat4>& GlobalPose() const { return m_globalPose; }
    std::vector<glm::vec3> BuildDebugLineVertices(const glm::mat4& debugTransform = glm::mat4(1.0f)) const;
    std::string ValidationMessage() const { return m_validationMessage; }
    float BoneMovementDistance(int boneIndex) const;

private:
    void EvaluatePose();
    bool ValidateClip(const AnimationClip& clip);

    const Skeleton* m_skeleton = nullptr;
    const std::vector<AnimationClip>* m_clips = nullptr;
    int m_activeClipIndex = -1;
    bool m_isPlaying = true;
    bool m_looping = true;
    float m_playbackSpeed = 1.0f;
    float m_currentTimeSeconds = 0.0f;
    std::vector<glm::mat4> m_globalPose;
    std::string m_validationMessage;
};
}
