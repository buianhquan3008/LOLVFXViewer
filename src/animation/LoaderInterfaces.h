#pragma once

#include "animation/AnimationClip.h"
#include "animation/Skeleton.h"

namespace lol
{
template <typename T>
struct Expected
{
    bool success = false;
    T value{};
    std::string error;
};

class ISkeletonLoader
{
public:
    virtual ~ISkeletonLoader() = default;
    virtual Expected<Skeleton> Load(const std::filesystem::path& path) = 0;
};

class IAnimationLoader
{
public:
    virtual ~IAnimationLoader() = default;
    virtual Expected<AnimationClip> Load(const std::filesystem::path& path) = 0;
};
}
