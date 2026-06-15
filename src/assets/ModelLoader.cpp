#include "assets/ModelLoader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <limits>

namespace lol
{
namespace
{
void AddBoneInfluence(Vertex& vertex, int boneIndex, float weight)
{
    if (weight <= 0.0f)
    {
        return;
    }

    for (int slot = 0; slot < 4; ++slot)
    {
        if (vertex.boneWeights[slot] == 0.0f)
        {
            vertex.boneIndices[slot] = boneIndex;
            vertex.boneWeights[slot] = weight;
            return;
        }
    }

    int smallestIndex = 0;
    for (int slot = 1; slot < 4; ++slot)
    {
        if (vertex.boneWeights[slot] < vertex.boneWeights[smallestIndex])
        {
            smallestIndex = slot;
        }
    }

    if (weight > vertex.boneWeights[smallestIndex])
    {
        vertex.boneIndices[smallestIndex] = boneIndex;
        vertex.boneWeights[smallestIndex] = weight;
    }
}

void NormalizeBoneWeights(Vertex& vertex)
{
    const float weightSum =
        vertex.boneWeights.x +
        vertex.boneWeights.y +
        vertex.boneWeights.z +
        vertex.boneWeights.w;

    if (weightSum > 0.0f)
    {
        vertex.boneWeights /= weightSum;
    }
}

glm::mat4 ToGlm(const aiMatrix4x4& matrix)
{
    glm::mat4 result;
    result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
    result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
    result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
    result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
    return result;
}

void BuildSkeletonRecursive(const aiNode* node, int parentIndex, Skeleton& skeleton, const glm::mat4& parentGlobal)
{
    const glm::mat4 local = ToGlm(node->mTransformation);
    glm::vec3 skew{};
    glm::vec4 perspective{};
    Bone bone;
    bone.name = node->mName.C_Str();
    bone.parentIndex = parentIndex;
    bone.localBindTransform = local;
    glm::decompose(local, bone.bindScale, bone.bindRotation, bone.bindPosition, skew, perspective);
    bone.globalBindTransform = parentGlobal * local;
    bone.inverseBindTransform = glm::inverse(bone.globalBindTransform);

    const int currentIndex = static_cast<int>(skeleton.bones.size());
    skeleton.boneLookup[bone.name] = currentIndex;
    skeleton.bones.push_back(bone);

    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        BuildSkeletonRecursive(node->mChildren[childIndex], currentIndex, skeleton, bone.globalBindTransform);
    }
}

template <typename TAssimp, typename TGlm>
std::vector<AnimationKey<TGlm>> ConvertKeys(const TAssimp* keys, unsigned int count, float ticksPerSecond);

template <>
std::vector<AnimationKey<glm::vec3>> ConvertKeys<aiVectorKey, glm::vec3>(const aiVectorKey* keys, unsigned int count, float ticksPerSecond)
{
    std::vector<AnimationKey<glm::vec3>> result;
    result.reserve(count);
    const float divisor = ticksPerSecond > 0.0f ? ticksPerSecond : 25.0f;
    for (unsigned int index = 0; index < count; ++index)
    {
        result.push_back(AnimationKey<glm::vec3>{
            .timeSeconds = static_cast<float>(keys[index].mTime / divisor),
            .value = glm::vec3(keys[index].mValue.x, keys[index].mValue.y, keys[index].mValue.z),
        });
    }
    return result;
}

template <>
std::vector<AnimationKey<glm::quat>> ConvertKeys<aiQuatKey, glm::quat>(const aiQuatKey* keys, unsigned int count, float ticksPerSecond)
{
    std::vector<AnimationKey<glm::quat>> result;
    result.reserve(count);
    const float divisor = ticksPerSecond > 0.0f ? ticksPerSecond : 25.0f;
    for (unsigned int index = 0; index < count; ++index)
    {
        result.push_back(AnimationKey<glm::quat>{
            .timeSeconds = static_cast<float>(keys[index].mTime / divisor),
            .value = glm::normalize(glm::quat(keys[index].mValue.w, keys[index].mValue.x, keys[index].mValue.y, keys[index].mValue.z)),
        });
    }
    return result;
}
}

std::optional<ModelData> ModelLoader::Load(const std::filesystem::path& path, std::string& errorMessage)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path.string(),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality
    );

    if (scene == nullptr || scene->mRootNode == nullptr)
    {
        errorMessage = importer.GetErrorString();
        return std::nullopt;
    }

    ModelData model;
    model.path = path;
    model.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    model.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    model.globalInverseTransform = glm::inverse(ToGlm(scene->mRootNode->mTransformation));
    BuildSkeletonRecursive(scene->mRootNode, -1, model.skeleton, glm::mat4(1.0f));

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        MeshData& outMesh = model.meshes.emplace_back();
        outMesh.vertices.reserve(mesh->mNumVertices);
        outMesh.indices.reserve(mesh->mNumFaces * 3);

        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
        {
            const aiVector3D& position = mesh->mVertices[vertexIndex];
            const aiVector3D& normal = mesh->mNormals[vertexIndex];
            const aiVector3D texCoord = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertexIndex] : aiVector3D();
            outMesh.vertices.push_back(Vertex{
                .position = {position.x, position.y, position.z},
                .normal = {normal.x, normal.y, normal.z},
                .texCoord = {texCoord.x, texCoord.y},
            });

            model.boundsMin = glm::min(model.boundsMin, outMesh.vertices.back().position);
            model.boundsMax = glm::max(model.boundsMax, outMesh.vertices.back().position);
        }

        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
        {
            const aiFace& face = mesh->mFaces[faceIndex];
            for (unsigned int index = 0; index < face.mNumIndices; ++index)
            {
                outMesh.indices.push_back(face.mIndices[index]);
            }
        }

        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
        {
            const aiBone* bone = mesh->mBones[boneIndex];
            const int skeletonBoneIndex = model.skeleton.FindBoneIndex(bone->mName.C_Str());
            if (skeletonBoneIndex < 0)
            {
                continue;
            }

            model.skeleton.bones[skeletonBoneIndex].inverseBindTransform = ToGlm(bone->mOffsetMatrix);

            for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
            {
                const aiVertexWeight& weight = bone->mWeights[weightIndex];
                if (weight.mVertexId < outMesh.vertices.size())
                {
                    AddBoneInfluence(outMesh.vertices[weight.mVertexId], skeletonBoneIndex, weight.mWeight);
                    model.weightedBoneIndices.insert(skeletonBoneIndex);
                }
            }
        }
    }

    for (unsigned int animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex)
    {
        const aiAnimation* animation = scene->mAnimations[animationIndex];
        AnimationClip clip;
        clip.name = animation->mName.length > 0 ? animation->mName.C_Str() : ("Animation " + std::to_string(animationIndex));
        clip.ticksPerSecond = static_cast<float>(animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 25.0);
        clip.durationSeconds = static_cast<float>(animation->mDuration / clip.ticksPerSecond);
        if (!std::isfinite(clip.durationSeconds) || clip.durationSeconds < 0.0f)
        {
            clip.durationSeconds = 0.0f;
        }

        for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
        {
            const aiNodeAnim* channel = animation->mChannels[channelIndex];
            BoneAnimationTrack track;
            track.boneName = channel->mNodeName.C_Str();
            track.positionKeys = ConvertKeys<aiVectorKey, glm::vec3>(channel->mPositionKeys, channel->mNumPositionKeys, clip.ticksPerSecond);
            track.rotationKeys = ConvertKeys<aiQuatKey, glm::quat>(channel->mRotationKeys, channel->mNumRotationKeys, clip.ticksPerSecond);
            track.scaleKeys = ConvertKeys<aiVectorKey, glm::vec3>(channel->mScalingKeys, channel->mNumScalingKeys, clip.ticksPerSecond);
            clip.tracks.emplace(track.boneName, std::move(track));
        }

        model.animationClips.push_back(std::move(clip));
    }

    for (MeshData& meshData : model.meshes)
    {
        for (Vertex& vertex : meshData.vertices)
        {
            NormalizeBoneWeights(vertex);
        }
    }

    return model;
}
}
