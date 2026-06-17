#include "assets/ModelLoader.h"
#include "core/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_set>

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

glm::mat4 NormalMatrixFromTransform(const glm::mat4& transform)
{
    return glm::transpose(glm::inverse(transform));
}

float MatrixMaxAbsDiff(const glm::mat4& lhs, const glm::mat4& rhs)
{
    float maxDiff = 0.0f;
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            maxDiff = std::max(maxDiff, std::abs(lhs[column][row] - rhs[column][row]));
        }
    }
    return maxDiff;
}

void CollectSkinnedBoneNames(const aiScene* scene, std::unordered_set<std::string>& boneNames)
{
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
        {
            boneNames.insert(mesh->mBones[boneIndex]->mName.C_Str());
        }
    }
}

void BuildSkeletonFromSkinnedBonesRecursive(
    const aiNode* node,
    int parentBoneIndex,
    const glm::mat4& parentBoneGlobal,
    const glm::mat4& parentSceneGlobal,
    const std::unordered_set<std::string>& skinnedBoneNames,
    Skeleton& skeleton)
{
    const glm::mat4 sceneLocal = ToGlm(node->mTransformation);
    const glm::mat4 sceneGlobal = parentSceneGlobal * sceneLocal;
    const std::string nodeName = node->mName.C_Str();
    const bool isSkinnedBone = skinnedBoneNames.contains(nodeName);

    int currentParentBoneIndex = parentBoneIndex;
    glm::mat4 currentParentBoneGlobal = parentBoneGlobal;

    if (isSkinnedBone)
    {
        if (skeleton.boneLookup.contains(nodeName))
        {
            Log::Push(LogLevel::Warning, "Duplicate skinned bone node name: " + nodeName);
        }
        else
        {
            const glm::mat4 local = glm::inverse(parentBoneGlobal) * sceneGlobal;
            const int currentIndex = static_cast<int>(skeleton.bones.size());

            glm::vec3 skew{};
            glm::vec4 perspective{};
            Bone bone;
            bone.name = nodeName;
            bone.parentIndex = parentBoneIndex;
            bone.isBone = true;
            bone.localBindTransform = local;
            glm::decompose(local, bone.bindScale, bone.bindRotation, bone.bindPosition, skew, perspective);
            bone.globalBindTransform = sceneGlobal;
            bone.inverseBindTransform = glm::inverse(sceneGlobal);

            skeleton.boneLookup[bone.name] = currentIndex;
            skeleton.bones.push_back(bone);

            currentParentBoneIndex = currentIndex;
            currentParentBoneGlobal = sceneGlobal;
        }
    }

    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        BuildSkeletonFromSkinnedBonesRecursive(
            node->mChildren[childIndex],
            currentParentBoneIndex,
            currentParentBoneGlobal,
            sceneGlobal,
            skinnedBoneNames,
            skeleton);
    }
}

void BuildSkeletonFromSkinnedBones(const aiScene* scene, Skeleton& skeleton)
{
    std::unordered_set<std::string> skinnedBoneNames;
    CollectSkinnedBoneNames(scene, skinnedBoneNames);

    BuildSkeletonFromSkinnedBonesRecursive(
        scene->mRootNode,
        -1,
        glm::mat4(1.0f),
        glm::mat4(1.0f),
        skinnedBoneNames,
        skeleton);

    if (skeleton.bones.size() != skinnedBoneNames.size())
    {
        std::ostringstream warning;
        warning << "Skeleton builder found " << skeleton.bones.size()
                << " of " << skinnedBoneNames.size()
                << " skinned bone nodes in the scene hierarchy.";
        Log::Push(LogLevel::Warning, warning.str());
    }
}

void CollectMeshNodeTransforms(
    const aiNode* node,
    const glm::mat4& parentGlobal,
    std::vector<glm::mat4>& meshGlobalTransforms)
{
    const glm::mat4 global = parentGlobal * ToGlm(node->mTransformation);

    for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
    {
        const unsigned int sceneMeshIndex = node->mMeshes[meshIndex];
        if (sceneMeshIndex < meshGlobalTransforms.size())
        {
            meshGlobalTransforms[sceneMeshIndex] = global;
        }
    }

    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        CollectMeshNodeTransforms(node->mChildren[childIndex], global, meshGlobalTransforms);
    }
}

void LogBindPoseDiagnostics(const ModelData& model)
{
    std::ostringstream summary;
    summary << "Skinning diagnostics";

    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const MeshData& mesh = model.meshes[meshIndex];
        double totalDisplacement = 0.0;
        float maxDisplacement = 0.0f;
        std::size_t weightedVertexCount = 0;

        for (const Vertex& vertex : mesh.vertices)
        {
            const float totalWeight =
                vertex.boneWeights.x +
                vertex.boneWeights.y +
                vertex.boneWeights.z +
                vertex.boneWeights.w;

            if (totalWeight <= 0.0f)
            {
                continue;
            }

            glm::mat4 skinMatrix(0.0f);
            for (int slot = 0; slot < 4; ++slot)
            {
                const int boneIndex = vertex.boneIndices[slot];
                const float weight = vertex.boneWeights[slot];
                if (weight <= 0.0f || boneIndex < 0 || boneIndex >= static_cast<int>(model.skeleton.bones.size()))
                {
                    continue;
                }

                const glm::mat4 bindSkinMatrix =
                    model.globalInverseTransform *
                    model.skeleton.bones[boneIndex].globalBindTransform *
                    mesh.boneOffsetMatrices[boneIndex];
                skinMatrix += bindSkinMatrix * weight;
            }

            const glm::vec3 transformed = glm::vec3(skinMatrix * glm::vec4(vertex.position, 1.0f));
            const float displacement = glm::length(transformed - vertex.position);
            totalDisplacement += displacement;
            maxDisplacement = std::max(maxDisplacement, displacement);
            ++weightedVertexCount;
        }

        summary << " | mesh " << meshIndex
                << ": weighted=" << weightedVertexCount
                << ", avgBindError=" << (weightedVertexCount > 0 ? totalDisplacement / static_cast<double>(weightedVertexCount) : 0.0)
                << ", maxBindError=" << maxDisplacement;
    }

    Log::Push(LogLevel::Info, summary.str(), model.path.string());
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
        aiProcess_ImproveCacheLocality |
        aiProcess_LimitBoneWeights
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
    BuildSkeletonFromSkinnedBones(scene, model.skeleton);

    std::vector<glm::mat4> meshGlobalTransforms(scene->mNumMeshes, glm::mat4(1.0f));
    CollectMeshNodeTransforms(scene->mRootNode, glm::mat4(1.0f), meshGlobalTransforms);
    int differingMeshOffsetCount = 0;
    std::vector<std::string> differingMeshOffsetSamples;

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        MeshData& outMesh = model.meshes.emplace_back();
        outMesh.vertices.reserve(mesh->mNumVertices);
        outMesh.indices.reserve(mesh->mNumFaces * 3);
        outMesh.meshToModelSpace = model.globalInverseTransform * meshGlobalTransforms[meshIndex];
        outMesh.boneOffsetMatrices.resize(model.skeleton.bones.size(), glm::mat4(1.0f));
        const glm::mat3 meshNormalToModelSpace = glm::mat3(NormalMatrixFromTransform(outMesh.meshToModelSpace));

        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
        {
            const aiVector3D& position = mesh->mVertices[vertexIndex];
            const aiVector3D& normal = mesh->mNormals[vertexIndex];
            const aiVector3D texCoord = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertexIndex] : aiVector3D();
            const glm::vec3 transformedPosition = glm::vec3(outMesh.meshToModelSpace * glm::vec4(position.x, position.y, position.z, 1.0f));
            const glm::vec3 transformedNormal = glm::normalize(meshNormalToModelSpace * glm::vec3(normal.x, normal.y, normal.z));
            outMesh.vertices.push_back(Vertex{
                .position = transformedPosition,
                .normal = transformedNormal,
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

            const glm::mat4 meshSpaceToBoneSpace = ToGlm(bone->mOffsetMatrix);
            if (model.skeleton.bones[skeletonBoneIndex].isBone)
            {
                const float diff = MatrixMaxAbsDiff(model.skeleton.bones[skeletonBoneIndex].inverseBindTransform, meshSpaceToBoneSpace);
                if (diff > 0.001f)
                {
                    ++differingMeshOffsetCount;
                    if (differingMeshOffsetSamples.size() < 5)
                    {
                        std::ostringstream sample;
                        sample << bone->mName.C_Str() << " (mesh " << meshIndex << ", diff=" << diff << ")";
                        differingMeshOffsetSamples.push_back(sample.str());
                    }
                }
            }

            model.skeleton.bones[skeletonBoneIndex].inverseBindTransform = meshSpaceToBoneSpace;
            model.skeleton.bones[skeletonBoneIndex].isBone = true;
            outMesh.boneOffsetMatrices[skeletonBoneIndex] = meshSpaceToBoneSpace * glm::inverse(outMesh.meshToModelSpace);

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

    if (differingMeshOffsetCount > 0)
    {
        std::ostringstream warning;
        warning << "Detected " << differingMeshOffsetCount
                << " mesh-specific bone offset mismatches. Samples: ";
        for (std::size_t sampleIndex = 0; sampleIndex < differingMeshOffsetSamples.size(); ++sampleIndex)
        {
            if (sampleIndex > 0)
            {
                warning << ", ";
            }
            warning << differingMeshOffsetSamples[sampleIndex];
        }
        Log::Push(LogLevel::Warning, warning.str(), path.string());
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

    LogBindPoseDiagnostics(model);

    return model;
}
}
