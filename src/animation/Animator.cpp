#include "animation/Animator.h"

#include "core/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace lol
{
namespace
{
bool IsFinite(float value)
{
    return std::isfinite(value);
}

bool IsFinite(const glm::vec3& value)
{
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

bool IsFinite(const glm::quat& value)
{
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z) && IsFinite(value.w);
}

glm::mat4 ComposeTransform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
{
    return glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

template <typename T>
T SampleTrack(const std::vector<AnimationKey<T>>& keys, float timeSeconds)
{
    if (keys.empty())
    {
        return T{};
    }

    if (keys.size() == 1 || timeSeconds <= keys.front().timeSeconds)
    {
        return keys.front().value;
    }

    if (timeSeconds >= keys.back().timeSeconds)
    {
        return keys.back().value;
    }

    for (std::size_t index = 0; index + 1 < keys.size(); ++index)
    {
        const auto& lhs = keys[index];
        const auto& rhs = keys[index + 1];
        if (timeSeconds >= lhs.timeSeconds && timeSeconds <= rhs.timeSeconds)
        {
            const float span = rhs.timeSeconds - lhs.timeSeconds;
            const float t = span <= 0.0f ? 0.0f : (timeSeconds - lhs.timeSeconds) / span;
            if constexpr (std::is_same_v<T, glm::quat>)
            {
                return glm::normalize(glm::slerp(lhs.value, rhs.value, t));
            }
            else
            {
                return glm::mix(lhs.value, rhs.value, t);
            }
        }
    }

    return keys.back().value;
}
}

void Animator::Clear()
{
    m_skeleton = nullptr;
    m_clips = nullptr;
    m_activeClipIndex = -1;
    m_currentTimeSeconds = 0.0f;
    m_globalPose.clear();
    m_validationMessage.clear();
}

void Animator::SetData(const Skeleton* skeleton, const std::vector<AnimationClip>* clips)
{
    m_skeleton = skeleton;
    m_clips = clips;
    m_currentTimeSeconds = 0.0f;
    m_activeClipIndex = (clips != nullptr && !clips->empty()) ? 0 : -1;
    m_validationMessage.clear();
    EvaluatePose();
}

void Animator::Update(float deltaSeconds)
{
    if (!m_isPlaying || m_activeClipIndex < 0)
    {
        EvaluatePose();
        return;
    }

    Step(deltaSeconds * m_playbackSpeed);
}

void Animator::SetActiveClip(int clipIndex)
{
    if (m_clips == nullptr || clipIndex < 0 || clipIndex >= static_cast<int>(m_clips->size()))
    {
        m_activeClipIndex = -1;
        m_currentTimeSeconds = 0.0f;
        m_validationMessage = "Selected clip index is invalid.";
        EvaluatePose();
        return;
    }

    m_activeClipIndex = clipIndex;
    m_currentTimeSeconds = 0.0f;
    m_validationMessage.clear();
    EvaluatePose();
}

void Animator::SetCurrentTime(float timeSeconds)
{
    m_currentTimeSeconds = std::max(timeSeconds, 0.0f);
    const AnimationClip* clip = ActiveClip();
    if (clip != nullptr && clip->durationSeconds > 0.0f)
    {
        m_currentTimeSeconds = std::clamp(m_currentTimeSeconds, 0.0f, clip->durationSeconds);
    }
    EvaluatePose();
}

void Animator::Step(float deltaSeconds)
{
    const AnimationClip* clip = ActiveClip();
    if (clip == nullptr || clip->durationSeconds <= 0.0f)
    {
        if (clip != nullptr && clip->durationSeconds <= 0.0f)
        {
            m_validationMessage = "Animation clip has invalid duration and cannot be played.";
        }
        EvaluatePose();
        return;
    }

    if (!ValidateClip(*clip))
    {
        m_isPlaying = false;
        EvaluatePose();
        return;
    }

    m_currentTimeSeconds += deltaSeconds;
    if (m_looping)
    {
        while (m_currentTimeSeconds < 0.0f)
        {
            m_currentTimeSeconds += clip->durationSeconds;
        }
        while (m_currentTimeSeconds > clip->durationSeconds)
        {
            m_currentTimeSeconds -= clip->durationSeconds;
        }
    }
    else
    {
        m_currentTimeSeconds = std::clamp(m_currentTimeSeconds, 0.0f, clip->durationSeconds);
        if (m_currentTimeSeconds >= clip->durationSeconds)
        {
            m_isPlaying = false;
        }
    }

    EvaluatePose();
}

bool Animator::HasSkeleton() const
{
    return m_skeleton != nullptr && !m_skeleton->bones.empty();
}

bool Animator::HasClips() const
{
    return m_clips != nullptr && !m_clips->empty();
}

const AnimationClip* Animator::ActiveClip() const
{
    if (m_clips == nullptr || m_activeClipIndex < 0 || m_activeClipIndex >= static_cast<int>(m_clips->size()))
    {
        return nullptr;
    }
    return &(*m_clips)[m_activeClipIndex];
}

std::vector<glm::vec3> Animator::BuildDebugLineVertices(const glm::mat4& debugTransform) const
{
    std::vector<glm::vec3> vertices;
    if (!HasSkeleton() || m_globalPose.size() != m_skeleton->bones.size())
    {
        return vertices;
    }

    for (std::size_t boneIndex = 0; boneIndex < m_skeleton->bones.size(); ++boneIndex)
    {
        const Bone& bone = m_skeleton->bones[boneIndex];
        if (bone.parentIndex < 0)
        {
            continue;
        }

        const glm::vec3 childPosition = glm::vec3(debugTransform * m_globalPose[boneIndex] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        const glm::vec3 parentPosition = glm::vec3(debugTransform * m_globalPose[bone.parentIndex] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(parentPosition);
        vertices.push_back(childPosition);
    }

    return vertices;
}

void Animator::EvaluatePose()
{
    m_globalPose.clear();
    if (!HasSkeleton())
    {
        return;
    }

    m_globalPose.resize(m_skeleton->bones.size(), glm::mat4(1.0f));
    const AnimationClip* clip = ActiveClip();

    if (clip != nullptr && !ValidateClip(*clip))
    {
        clip = nullptr;
    }

    for (std::size_t boneIndex = 0; boneIndex < m_skeleton->bones.size(); ++boneIndex)
    {
        const Bone& bone = m_skeleton->bones[boneIndex];
        glm::vec3 position = bone.bindPosition;
        glm::quat rotation = bone.bindRotation;
        glm::vec3 scale = bone.bindScale;
        glm::mat4 local = bone.localBindTransform;
        bool hasAnimatedChannel = false;

        if (clip != nullptr)
        {
            const auto trackIt = clip->tracks.find(bone.name);
            if (trackIt != clip->tracks.end())
            {
                hasAnimatedChannel = true;
                const BoneAnimationTrack& track = trackIt->second;
                if (!track.positionKeys.empty())
                {
                    position = SampleTrack(track.positionKeys, m_currentTimeSeconds);
                    if (!IsFinite(position))
                    {
                        position = bone.bindPosition;
                    }
                }
                if (!track.rotationKeys.empty())
                {
                    rotation = SampleTrack(track.rotationKeys, m_currentTimeSeconds);
                    if (!IsFinite(rotation) || glm::length(rotation) <= 0.0001f)
                    {
                        rotation = bone.bindRotation;
                    }
                }
                if (!track.scaleKeys.empty())
                {
                    scale = SampleTrack(track.scaleKeys, m_currentTimeSeconds);
                    if (!IsFinite(scale))
                    {
                        scale = bone.bindScale;
                    }
                }

                local = ComposeTransform(position, rotation, scale);
            }
        }

        if (!hasAnimatedChannel)
        {
            local = bone.localBindTransform;
        }

        if (bone.parentIndex >= 0)
        {
            m_globalPose[boneIndex] = m_globalPose[bone.parentIndex] * local;
        }
        else
        {
            m_globalPose[boneIndex] = local;
        }
    }
}

bool Animator::ValidateClip(const AnimationClip& clip)
{
    if (clip.durationSeconds <= 0.0f || !std::isfinite(clip.durationSeconds))
    {
        m_validationMessage = "Animation clip duration is invalid.";
        Log::Push(LogLevel::Warning, m_validationMessage + " Clip: " + clip.name);
        return false;
    }

    for (const auto& [boneName, track] : clip.tracks)
    {
        for (const auto& key : track.positionKeys)
        {
            if (!std::isfinite(key.timeSeconds) || !IsFinite(key.value))
            {
                m_validationMessage = "Animation position key contains invalid values.";
                Log::Push(LogLevel::Warning, m_validationMessage + " Bone: " + boneName + ", clip: " + clip.name);
                return false;
            }
        }
        for (const auto& key : track.rotationKeys)
        {
            if (!std::isfinite(key.timeSeconds) || !IsFinite(key.value))
            {
                m_validationMessage = "Animation rotation key contains invalid values.";
                Log::Push(LogLevel::Warning, m_validationMessage + " Bone: " + boneName + ", clip: " + clip.name);
                return false;
            }
        }
        for (const auto& key : track.scaleKeys)
        {
            if (!std::isfinite(key.timeSeconds) || !IsFinite(key.value))
            {
                m_validationMessage = "Animation scale key contains invalid values.";
                Log::Push(LogLevel::Warning, m_validationMessage + " Bone: " + boneName + ", clip: " + clip.name);
                return false;
            }
        }
    }

    return true;
}

float Animator::BoneMovementDistance(int boneIndex) const
{
    if (!HasSkeleton() || boneIndex < 0 || boneIndex >= static_cast<int>(m_globalPose.size()))
    {
        return 0.0f;
    }

    const glm::vec3 current = glm::vec3(m_globalPose[boneIndex][3]);
    const glm::vec3 bind = glm::vec3(m_skeleton->bones[boneIndex].globalBindTransform[3]);
    return glm::length(current - bind);
}
}
