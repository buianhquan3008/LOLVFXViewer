#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace lol
{
template <typename T>
struct AnimationKey
{
    float timeSeconds = 0.0f;
    T value{};
};

struct BoneAnimationTrack
{
    std::string boneName;
    std::vector<AnimationKey<glm::vec3>> positionKeys;
    std::vector<AnimationKey<glm::quat>> rotationKeys;
    std::vector<AnimationKey<glm::vec3>> scaleKeys;
};

struct AnimationClip
{
    std::string name;
    float durationSeconds = 0.0f;
    float ticksPerSecond = 25.0f;
    std::unordered_map<std::string, BoneAnimationTrack> tracks;
};
}
