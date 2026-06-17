#pragma once

#include "animation/AnimationClip.h"
#include "animation/Skeleton.h"

#include <filesystem>
#include <unordered_set>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace lol
{
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::ivec4 boneIndices = glm::ivec4(0);
    glm::vec4 boneWeights = glm::vec4(0.0f);
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::mat4 meshToModelSpace = glm::mat4(1.0f);
    std::vector<glm::mat4> boneOffsetMatrices;
};

struct ModelData
{
    std::filesystem::path path;
    std::vector<MeshData> meshes;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    glm::mat4 globalInverseTransform = glm::mat4(1.0f);
    Skeleton skeleton;
    std::vector<AnimationClip> animationClips;
    std::unordered_set<int> weightedBoneIndices;
};

class ModelLoader
{
public:
    static std::optional<ModelData> Load(const std::filesystem::path& path, std::string& errorMessage);
};
}
