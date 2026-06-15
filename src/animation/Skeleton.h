#pragma once

#include "animation/Bone.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lol
{
struct Skeleton
{
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneLookup;

    int FindBoneIndex(const std::string& name) const
    {
        const auto it = boneLookup.find(name);
        return it == boneLookup.end() ? -1 : it->second;
    }

    bool Empty() const
    {
        return bones.empty();
    }
};
}
