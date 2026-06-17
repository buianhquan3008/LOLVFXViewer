#include "assets/RiotModelLoader.h"

#include "core/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace lol
{
namespace
{
constexpr std::uint32_t kSimpleSkinMagic = 0x00112233;
constexpr std::uint32_t kModernSklFormatToken = 0x22FD4FC3;

class BinaryReader
{
public:
    explicit BinaryReader(const std::filesystem::path& path)
        : m_stream(path, std::ios::binary)
    {
        if (!m_stream)
        {
            throw std::runtime_error("Failed to open file.");
        }
    }

    template <typename T>
    T Read()
    {
        T value{};
        ReadBytes(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }

    void ReadBytes(char* destination, std::size_t size)
    {
        m_stream.read(destination, static_cast<std::streamsize>(size));
        if (m_stream.gcount() != static_cast<std::streamsize>(size))
        {
            throw std::runtime_error("Unexpected end of file.");
        }
    }

    std::string ReadFixedString(std::size_t size)
    {
        std::string value(size, '\0');
        ReadBytes(value.data(), size);
        const std::size_t end = value.find('\0');
        if (end != std::string::npos)
        {
            value.resize(end);
        }
        return value;
    }

    std::string ReadNullTerminatedString()
    {
        std::string value;
        char c = '\0';
        while (m_stream.get(c))
        {
            if (c == '\0')
            {
                return value;
            }
            value.push_back(c);
        }
        throw std::runtime_error("Unexpected end of file while reading string.");
    }

    glm::vec2 ReadVec2()
    {
        return {Read<float>(), Read<float>()};
    }

    glm::vec3 ReadVec3()
    {
        return {Read<float>(), Read<float>(), Read<float>()};
    }

    glm::quat ReadQuat()
    {
        const float x = Read<float>();
        const float y = Read<float>();
        const float z = Read<float>();
        const float w = Read<float>();
        return glm::normalize(glm::quat(w, x, y, z));
    }

    void Seek(std::streamoff offset)
    {
        m_stream.seekg(offset, std::ios::beg);
        if (!m_stream)
        {
            throw std::runtime_error("Failed to seek file.");
        }
    }

    std::streamoff Tell()
    {
        return m_stream.tellg();
    }

    std::streamoff Size()
    {
        const std::streamoff current = Tell();
        m_stream.seekg(0, std::ios::end);
        const std::streamoff size = Tell();
        Seek(current);
        return size;
    }

private:
    std::ifstream m_stream;
};

struct RiotJoint
{
    std::string name;
    int id = -1;
    int parentId = -1;
    glm::mat4 localTransform = glm::mat4(1.0f);
    glm::mat4 inverseBindTransform = glm::mat4(1.0f);
};

struct RiotRig
{
    std::vector<RiotJoint> joints;
    std::vector<int> influences;
};

struct RiotRange
{
    std::string material;
    int startVertex = 0;
    int vertexCount = 0;
    int startIndex = 0;
    int indexCount = 0;
};

struct RiotAnimationFrame
{
    std::uint16_t translationId = 0;
    std::uint16_t scaleId = 0;
    std::uint16_t rotationId = 0;
};

std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::uint32_t ElfHashLower(const std::string& value)
{
    std::uint32_t hash = 0;
    for (unsigned char c : Lowercase(value))
    {
        hash = (hash << 4) + c;
        const std::uint32_t high = hash & 0xF0000000u;
        if (high != 0)
        {
            hash ^= high >> 24;
        }
        hash &= ~high;
    }
    return hash;
}

glm::mat4 ComposeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
{
    return glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

glm::quat DecompressQuantizedQuaternion(const std::array<std::uint8_t, 6>& data)
{
    constexpr double sqrt2 = 1.41421356237;
    constexpr double oneOverSqrt2 = 0.70710678118;

    const std::uint64_t bits =
        static_cast<std::uint64_t>(data[0]) |
        (static_cast<std::uint64_t>(data[1]) << 8) |
        (static_cast<std::uint64_t>(data[2]) << 16) |
        (static_cast<std::uint64_t>(data[3]) << 24) |
        (static_cast<std::uint64_t>(data[4]) << 32) |
        (static_cast<std::uint64_t>(data[5]) << 40);

    const std::uint16_t maxIndex = static_cast<std::uint16_t>((bits >> 45) & 0x0003u);
    const std::uint16_t va = static_cast<std::uint16_t>((bits >> 30) & 0x7FFFu);
    const std::uint16_t vb = static_cast<std::uint16_t>((bits >> 15) & 0x7FFFu);
    const std::uint16_t vc = static_cast<std::uint16_t>(bits & 0x7FFFu);

    const float a = static_cast<float>(va / 32767.0 * sqrt2 - oneOverSqrt2);
    const float b = static_cast<float>(vb / 32767.0 * sqrt2 - oneOverSqrt2);
    const float c = static_cast<float>(vc / 32767.0 * sqrt2 - oneOverSqrt2);
    const float d = std::sqrt(std::max(0.0f, 1.0f - (a * a + b * b + c * c)));

    switch (maxIndex)
    {
    case 0: return glm::normalize(glm::quat(c, d, a, b));
    case 1: return glm::normalize(glm::quat(c, a, d, b));
    case 2: return glm::normalize(glm::quat(c, a, b, d));
    default: return glm::normalize(glm::quat(d, a, b, c));
    }
}

glm::mat4 ReadLegacyGlobalTransform(BinaryReader& reader)
{
    float transform[4][4]{};
    transform[0][3] = 0.0f;
    transform[1][3] = 0.0f;
    transform[2][3] = 0.0f;
    transform[3][3] = 1.0f;

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            transform[column][row] = reader.Read<float>();
        }
    }

    glm::mat4 matrix(1.0f);
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            matrix[column][row] = transform[column][row];
        }
    }
    return matrix;
}

RiotRig LoadSkl(const std::filesystem::path& path)
{
    BinaryReader reader(path);
    reader.Seek(4);
    const std::uint32_t possibleFormatToken = reader.Read<std::uint32_t>();
    reader.Seek(0);

    RiotRig rig;
    if (possibleFormatToken == kModernSklFormatToken)
    {
        [[maybe_unused]] const std::uint32_t fileSize = reader.Read<std::uint32_t>();
        [[maybe_unused]] const std::uint32_t formatToken = reader.Read<std::uint32_t>();
        const std::uint32_t version = reader.Read<std::uint32_t>();
        if (version != 0)
        {
            throw std::runtime_error("Unsupported modern SKL version.");
        }

        [[maybe_unused]] const std::uint16_t flags = reader.Read<std::uint16_t>();
        const std::uint16_t jointCount = reader.Read<std::uint16_t>();
        const std::uint32_t influenceCount = reader.Read<std::uint32_t>();
        const std::int32_t jointsOffset = reader.Read<std::int32_t>();
        [[maybe_unused]] const std::int32_t jointIndicesOffset = reader.Read<std::int32_t>();
        const std::int32_t influencesOffset = reader.Read<std::int32_t>();
        [[maybe_unused]] const std::int32_t nameOffset = reader.Read<std::int32_t>();
        [[maybe_unused]] const std::int32_t assetNameOffset = reader.Read<std::int32_t>();
        [[maybe_unused]] const std::int32_t boneNamesOffset = reader.Read<std::int32_t>();
        for (int i = 0; i < 5; ++i)
        {
            (void)reader.Read<std::int32_t>();
        }

        rig.joints.resize(jointCount);
        if (jointsOffset > 0)
        {
            reader.Seek(jointsOffset);
            for (int i = 0; i < jointCount; ++i)
            {
                [[maybe_unused]] const std::uint16_t jointFlags = reader.Read<std::uint16_t>();
                const std::int16_t id = reader.Read<std::int16_t>();
                const std::int16_t parentId = reader.Read<std::int16_t>();
                (void)reader.Read<std::int16_t>();
                [[maybe_unused]] const std::uint32_t nameHash = reader.Read<std::uint32_t>();
                [[maybe_unused]] const float radius = reader.Read<float>();

                const glm::vec3 localTranslation = reader.ReadVec3();
                const glm::vec3 localScale = reader.ReadVec3();
                const glm::quat localRotation = reader.ReadQuat();
                const glm::vec3 inverseBindTranslation = reader.ReadVec3();
                const glm::vec3 inverseBindScale = reader.ReadVec3();
                const glm::quat inverseBindRotation = reader.ReadQuat();

                const std::int32_t relativeNameOffset = reader.Read<std::int32_t>();
                const std::streamoff returnOffset = reader.Tell();
                reader.Seek(returnOffset - 4 + relativeNameOffset);
                const std::string name = reader.ReadNullTerminatedString();
                reader.Seek(returnOffset);

                rig.joints[i] = RiotJoint{
                    .name = name,
                    .id = id,
                    .parentId = parentId,
                    .localTransform = ComposeTransform(localTranslation, localRotation, localScale),
                    .inverseBindTransform = ComposeTransform(inverseBindTranslation, inverseBindRotation, inverseBindScale),
                };
            }
        }

        rig.influences.resize(influenceCount);
        if (influencesOffset > 0)
        {
            reader.Seek(influencesOffset);
            for (std::uint32_t i = 0; i < influenceCount; ++i)
            {
                rig.influences[i] = reader.Read<std::int16_t>();
            }
        }
    }
    else
    {
        const std::string magic = reader.ReadFixedString(8);
        if (magic != "r3d2sklt")
        {
            throw std::runtime_error("Invalid SKL signature.");
        }

        const std::uint32_t version = reader.Read<std::uint32_t>();
        if (version != 1 && version != 2)
        {
            throw std::runtime_error("Unsupported legacy SKL version.");
        }

        (void)reader.Read<std::uint32_t>();
        const std::uint32_t jointCount = reader.Read<std::uint32_t>();
        std::vector<glm::mat4> globalTransforms(jointCount, glm::mat4(1.0f));
        rig.joints.resize(jointCount);
        for (std::uint32_t i = 0; i < jointCount; ++i)
        {
            const std::string name = reader.ReadFixedString(32);
            const int parentId = reader.Read<std::int32_t>();
            (void)reader.Read<float>();
            const glm::mat4 globalTransform = ReadLegacyGlobalTransform(reader);
            globalTransforms[i] = globalTransform;

            glm::mat4 localTransform = globalTransform;
            if (parentId >= 0 && parentId < static_cast<int>(globalTransforms.size()))
            {
                localTransform = glm::inverse(globalTransforms[parentId]) * globalTransform;
            }

            rig.joints[i] = RiotJoint{
                .name = name,
                .id = static_cast<int>(i),
                .parentId = parentId,
                .localTransform = localTransform,
                .inverseBindTransform = glm::inverse(globalTransform),
            };
        }

        if (version == 2)
        {
            const std::uint32_t influenceCount = reader.Read<std::uint32_t>();
            rig.influences.resize(influenceCount);
            for (std::uint32_t i = 0; i < influenceCount; ++i)
            {
                rig.influences[i] = static_cast<int>(reader.Read<std::uint32_t>());
            }
        }
        else
        {
            rig.influences.resize(jointCount);
            for (std::uint32_t i = 0; i < jointCount; ++i)
            {
                rig.influences[i] = static_cast<int>(i);
            }
        }
    }

    return rig;
}

void BuildSkeletonFromRig(const RiotRig& rig, ModelData& model)
{
    model.skeleton.bones.clear();
    model.skeleton.boneLookup.clear();
    model.skeleton.bones.resize(rig.joints.size());

    for (std::size_t i = 0; i < rig.joints.size(); ++i)
    {
        const RiotJoint& joint = rig.joints[i];
        Bone& bone = model.skeleton.bones[i];
        bone.name = joint.name;
        bone.parentIndex = joint.parentId;
        bone.isBone = true;
        bone.localBindTransform = joint.localTransform;
        bone.inverseBindTransform = joint.inverseBindTransform;

        glm::vec3 skew{};
        glm::vec4 perspective{};
        glm::decompose(bone.localBindTransform, bone.bindScale, bone.bindRotation, bone.bindPosition, skew, perspective);
        model.skeleton.boneLookup[bone.name] = static_cast<int>(i);
    }

    for (std::size_t i = 0; i < model.skeleton.bones.size(); ++i)
    {
        Bone& bone = model.skeleton.bones[i];
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(model.skeleton.bones.size()))
        {
            bone.globalBindTransform = model.skeleton.bones[bone.parentIndex].globalBindTransform * bone.localBindTransform;
        }
        else
        {
            bone.parentIndex = -1;
            bone.globalBindTransform = bone.localBindTransform;
        }
    }
}

void ExpandBounds(ModelData& model, const glm::vec3& position)
{
    model.boundsMin = glm::min(model.boundsMin, position);
    model.boundsMax = glm::max(model.boundsMax, position);
}

void LoadSkn(const std::filesystem::path& path, const RiotRig& rig, ModelData& model)
{
    BinaryReader reader(path);
    const std::uint32_t magic = reader.Read<std::uint32_t>();
    if (magic != kSimpleSkinMagic)
    {
        throw std::runtime_error("Invalid SKN signature.");
    }

    const std::uint16_t major = reader.Read<std::uint16_t>();
    const std::uint16_t minor = reader.Read<std::uint16_t>();
    if (!((major == 0 || major == 2 || major == 4) && minor == 1))
    {
        throw std::runtime_error("Unsupported SKN version.");
    }

    std::vector<RiotRange> ranges;
    int indexCount = 0;
    int vertexCount = 0;
    std::uint32_t vertexSize = 52;
    std::uint32_t vertexType = 0;

    if (major == 0)
    {
        indexCount = reader.Read<std::int32_t>();
        vertexCount = reader.Read<std::int32_t>();
        ranges.push_back(RiotRange{.material = "Base", .startVertex = 0, .vertexCount = vertexCount, .startIndex = 0, .indexCount = indexCount});
    }
    else
    {
        const std::uint32_t rangeCount = reader.Read<std::uint32_t>();
        ranges.resize(rangeCount);
        for (std::uint32_t i = 0; i < rangeCount; ++i)
        {
            ranges[i].material = reader.ReadFixedString(64);
            ranges[i].startVertex = reader.Read<std::int32_t>();
            ranges[i].vertexCount = reader.Read<std::int32_t>();
            ranges[i].startIndex = reader.Read<std::int32_t>();
            ranges[i].indexCount = reader.Read<std::int32_t>();
        }

        if (major == 4)
        {
            (void)reader.Read<std::uint32_t>();
        }

        indexCount = reader.Read<std::int32_t>();
        vertexCount = reader.Read<std::int32_t>();

        if (major == 4)
        {
            vertexSize = reader.Read<std::uint32_t>();
            vertexType = reader.Read<std::uint32_t>();
            for (int i = 0; i < 10; ++i)
            {
                (void)reader.Read<float>();
            }
        }
    }

    if (vertexSize != 52 && vertexSize != 56 && vertexSize != 72)
    {
        std::ostringstream stream;
        stream << "Unsupported SKN vertex size: " << vertexSize << ".";
        throw std::runtime_error(stream.str());
    }

    std::vector<unsigned int> allIndices(indexCount);
    for (int i = 0; i < indexCount; ++i)
    {
        allIndices[i] = reader.Read<std::uint16_t>();
    }

    std::vector<Vertex> allVertices(vertexCount);
    for (int i = 0; i < vertexCount; ++i)
    {
        Vertex vertex;
        vertex.position = reader.ReadVec3();

        const std::array<std::uint8_t, 4> blendIndexBytes = {
            reader.Read<std::uint8_t>(),
            reader.Read<std::uint8_t>(),
            reader.Read<std::uint8_t>(),
            reader.Read<std::uint8_t>(),
        };
        vertex.boneWeights = glm::vec4(reader.Read<float>(), reader.Read<float>(), reader.Read<float>(), reader.Read<float>());
        vertex.normal = glm::normalize(reader.ReadVec3());
        vertex.texCoord = reader.ReadVec2();

        if (vertexSize >= 56)
        {
            (void)reader.Read<std::uint32_t>();
        }
        if (vertexSize >= 72)
        {
            for (int component = 0; component < 4; ++component)
            {
                (void)reader.Read<float>();
            }
        }

        for (int slot = 0; slot < 4; ++slot)
        {
            const int influenceIndex = blendIndexBytes[slot];
            int jointId = influenceIndex;
            if (influenceIndex >= 0 && influenceIndex < static_cast<int>(rig.influences.size()))
            {
                jointId = rig.influences[influenceIndex];
            }

            vertex.boneIndices[slot] = jointId;
            if (vertex.boneWeights[slot] > 0.0f && jointId >= 0 && jointId < static_cast<int>(model.skeleton.bones.size()))
            {
                model.weightedBoneIndices.insert(jointId);
            }
        }

        allVertices[i] = vertex;
        ExpandBounds(model, vertex.position);
    }

    if (ranges.empty())
    {
        ranges.push_back(RiotRange{.material = "Base", .startVertex = 0, .vertexCount = vertexCount, .startIndex = 0, .indexCount = indexCount});
    }

    for (const RiotRange& range : ranges)
    {
        MeshData& mesh = model.meshes.emplace_back();
        mesh.vertices.reserve(range.vertexCount);
        mesh.indices.reserve(range.indexCount);
        mesh.boneOffsetMatrices.resize(model.skeleton.bones.size(), glm::mat4(1.0f));
        for (std::size_t boneIndex = 0; boneIndex < model.skeleton.bones.size(); ++boneIndex)
        {
            mesh.boneOffsetMatrices[boneIndex] = model.skeleton.bones[boneIndex].inverseBindTransform;
        }

        for (int i = 0; i < range.vertexCount; ++i)
        {
            const int sourceVertexIndex = range.startVertex + i;
            if (sourceVertexIndex >= 0 && sourceVertexIndex < static_cast<int>(allVertices.size()))
            {
                mesh.vertices.push_back(allVertices[sourceVertexIndex]);
            }
        }

        for (int i = 0; i < range.indexCount; ++i)
        {
            const int sourceIndexIndex = range.startIndex + i;
            if (sourceIndexIndex >= 0 && sourceIndexIndex < static_cast<int>(allIndices.size()))
            {
                const unsigned int sourceVertexIndex = allIndices[sourceIndexIndex];
                mesh.indices.push_back(sourceVertexIndex >= static_cast<unsigned int>(range.startVertex)
                    ? sourceVertexIndex - static_cast<unsigned int>(range.startVertex)
                    : sourceVertexIndex);
            }
        }
    }

    Log::Push(
        LogLevel::Info,
        "Loaded SKN mesh: vertices=" + std::to_string(vertexCount) +
            ", indices=" + std::to_string(indexCount) +
            ", ranges=" + std::to_string(ranges.size()) +
            ", vertexType=" + std::to_string(vertexType),
        path.string());
}

std::optional<std::filesystem::path> FindSiblingFile(const std::filesystem::path& path, const std::string& extension)
{
    const std::filesystem::path exact = path.parent_path() / (path.stem().string() + extension);
    if (std::filesystem::exists(exact))
    {
        return exact;
    }

    if (!std::filesystem::exists(path.parent_path()))
    {
        return std::nullopt;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path.parent_path()))
    {
        if (Lowercase(entry.path().extension().string()) == extension)
        {
            return entry.path();
        }
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> FindAnimationFiles(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> result;
    const std::array<std::filesystem::path, 2> folders = {
        path.parent_path(),
        path.parent_path() / "Animations",
    };

    for (const std::filesystem::path& folder : folders)
    {
        if (!std::filesystem::exists(folder) || !std::filesystem::is_directory(folder))
        {
            continue;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder))
        {
            if (Lowercase(entry.path().extension().string()) == ".anm")
            {
                result.push_back(entry.path());
            }
        }
    }

    std::ranges::sort(result);
    result.erase(std::ranges::unique(result).begin(), result.end());
    return result;
}

void AddAnimationTrackKey(
    AnimationClip& clip,
    const std::string& boneName,
    float timeSeconds,
    const glm::vec3& translation,
    const glm::quat& rotation,
    const glm::vec3& scale)
{
    BoneAnimationTrack& track = clip.tracks[boneName];
    track.boneName = boneName;
    track.positionKeys.push_back(AnimationKey<glm::vec3>{.timeSeconds = timeSeconds, .value = translation});
    track.rotationKeys.push_back(AnimationKey<glm::quat>{.timeSeconds = timeSeconds, .value = rotation});
    track.scaleKeys.push_back(AnimationKey<glm::vec3>{.timeSeconds = timeSeconds, .value = scale});
}

AnimationClip LoadUncompressedAnm(const std::filesystem::path& path, const Skeleton& skeleton)
{
    BinaryReader reader(path);
    const std::string magic = reader.ReadFixedString(8);
    const std::uint32_t fileVersion = reader.Read<std::uint32_t>();

    if (magic == "r3d2canm")
    {
        throw std::runtime_error("Compressed ANM is not supported by the first Riot loader pass.");
    }
    if (magic != "r3d2anmd")
    {
        throw std::runtime_error("Invalid ANM signature.");
    }

    AnimationClip clip;
    clip.name = path.stem().string();

    std::unordered_map<std::uint32_t, std::string> boneNamesByHash;
    for (const Bone& bone : skeleton.bones)
    {
        boneNamesByHash[ElfHashLower(bone.name)] = bone.name;
    }

    if (fileVersion == 3)
    {
        (void)reader.Read<std::uint32_t>();
        const int trackCount = reader.Read<std::int32_t>();
        const int frameCount = reader.Read<std::int32_t>();
        const int fps = reader.Read<std::int32_t>();
        clip.ticksPerSecond = static_cast<float>(fps);
        clip.durationSeconds = fps > 0 ? static_cast<float>(frameCount) / static_cast<float>(fps) : 0.0f;

        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        {
            const std::string trackName = reader.ReadFixedString(32);
            (void)reader.Read<std::uint32_t>();
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
            {
                const glm::quat rotation = reader.ReadQuat();
                const glm::vec3 translation = reader.ReadVec3();
                const float timeSeconds = fps > 0 ? static_cast<float>(frameIndex) / static_cast<float>(fps) : 0.0f;
                AddAnimationTrackKey(clip, trackName, timeSeconds, translation, rotation, glm::vec3(1.0f));
            }
        }
        return clip;
    }

    if (fileVersion != 4 && fileVersion != 5)
    {
        throw std::runtime_error("Unsupported ANM version.");
    }

    (void)reader.Read<std::uint32_t>();
    (void)reader.Read<std::uint32_t>();
    (void)reader.Read<std::uint32_t>();
    (void)reader.Read<std::uint32_t>();
    const int trackCount = reader.Read<std::int32_t>();
    const int frameCount = reader.Read<std::int32_t>();
    const float frameDuration = reader.Read<float>();
    clip.ticksPerSecond = frameDuration > 0.0f ? 1.0f / frameDuration : 25.0f;
    clip.durationSeconds = frameCount * frameDuration;

    const int jointNameHashesOffset = reader.Read<std::int32_t>();
    (void)reader.Read<std::int32_t>();
    (void)reader.Read<std::int32_t>();
    const int vectorPaletteOffset = reader.Read<std::int32_t>();
    const int quatPaletteOffset = reader.Read<std::int32_t>();
    const int framesOffset = reader.Read<std::int32_t>();

    std::vector<std::uint32_t> jointHashes(trackCount, 0);
    if (fileVersion == 5)
    {
        reader.Seek(jointNameHashesOffset + 12);
        for (int i = 0; i < trackCount; ++i)
        {
            jointHashes[i] = reader.Read<std::uint32_t>();
        }
    }

    const int vectorCount = (quatPaletteOffset - vectorPaletteOffset) / 12;
    std::vector<glm::vec3> vectorPalette(vectorCount);
    reader.Seek(vectorPaletteOffset + 12);
    for (int i = 0; i < vectorCount; ++i)
    {
        vectorPalette[i] = reader.ReadVec3();
    }

    std::vector<glm::quat> quatPalette;
    if (fileVersion == 4)
    {
        const int quatCount = (framesOffset - quatPaletteOffset) / 16;
        quatPalette.resize(quatCount);
        reader.Seek(quatPaletteOffset + 12);
        for (int i = 0; i < quatCount; ++i)
        {
            quatPalette[i] = reader.ReadQuat();
        }
    }
    else
    {
        const int quatCount = (jointNameHashesOffset - quatPaletteOffset) / 6;
        quatPalette.resize(quatCount);
        reader.Seek(quatPaletteOffset + 12);
        for (int i = 0; i < quatCount; ++i)
        {
            std::array<std::uint8_t, 6> bytes{};
            for (std::uint8_t& byte : bytes)
            {
                byte = reader.Read<std::uint8_t>();
            }
            quatPalette[i] = DecompressQuantizedQuaternion(bytes);
        }
    }

    reader.Seek(framesOffset + 12);
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        {
            std::uint32_t jointHash = 0;
            if (fileVersion == 4)
            {
                jointHash = reader.Read<std::uint32_t>();
            }
            else
            {
                jointHash = jointHashes[trackIndex];
            }

            RiotAnimationFrame frame;
            frame.translationId = reader.Read<std::uint16_t>();
            frame.scaleId = reader.Read<std::uint16_t>();
            frame.rotationId = reader.Read<std::uint16_t>();
            if (fileVersion == 4)
            {
                (void)reader.Read<std::uint16_t>();
            }

            const auto nameIt = boneNamesByHash.find(jointHash);
            if (nameIt == boneNamesByHash.end())
            {
                continue;
            }

            const glm::vec3 translation = frame.translationId < vectorPalette.size() ? vectorPalette[frame.translationId] : glm::vec3(0.0f);
            const glm::vec3 scale = frame.scaleId < vectorPalette.size() ? vectorPalette[frame.scaleId] : glm::vec3(1.0f);
            const glm::quat rotation = frame.rotationId < quatPalette.size() ? quatPalette[frame.rotationId] : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            AddAnimationTrackKey(clip, nameIt->second, frameIndex * frameDuration, translation, rotation, scale);
        }
    }

    return clip;
}

void LoadAnimationsNear(const std::filesystem::path& path, ModelData& model)
{
    for (const std::filesystem::path& animationPath : FindAnimationFiles(path))
    {
        try
        {
            AnimationClip clip = LoadUncompressedAnm(animationPath, model.skeleton);
            Log::Push(LogLevel::Info, "Loaded ANM animation: " + clip.name + ", tracks=" + std::to_string(clip.tracks.size()), animationPath.string());
            model.animationClips.push_back(std::move(clip));
        }
        catch (const std::exception& ex)
        {
            Log::Push(LogLevel::Warning, std::string("Skipped ANM animation: ") + ex.what(), animationPath.string());
        }
    }
}
}

bool RiotModelLoader::IsRiotModelPath(const std::filesystem::path& path)
{
    const std::string ext = Lowercase(path.extension().string());
    return ext == ".skn" || ext == ".skl";
}

std::optional<ModelData> RiotModelLoader::Load(const std::filesystem::path& path, std::string& errorMessage)
{
    try
    {
        const std::string ext = Lowercase(path.extension().string());
        std::optional<std::filesystem::path> sklPath;
        std::optional<std::filesystem::path> sknPath;

        if (ext == ".skl")
        {
            sklPath = path;
            sknPath = FindSiblingFile(path, ".skn");
        }
        else if (ext == ".skn")
        {
            sknPath = path;
            sklPath = FindSiblingFile(path, ".skl");
        }
        else
        {
            errorMessage = "Unsupported Riot model extension.";
            return std::nullopt;
        }

        if (!sklPath)
        {
            errorMessage = "Could not find a companion .skl skeleton file.";
            return std::nullopt;
        }

        ModelData model;
        model.path = path;
        model.globalInverseTransform = glm::mat4(1.0f);
        model.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        model.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

        RiotRig rig = LoadSkl(*sklPath);
        BuildSkeletonFromRig(rig, model);
        Log::Push(LogLevel::Info, "Loaded SKL skeleton: joints=" + std::to_string(model.skeleton.bones.size()), sklPath->string());

        if (sknPath)
        {
            LoadSkn(*sknPath, rig, model);
        }
        else
        {
            model.boundsMin = glm::vec3(-1.0f);
            model.boundsMax = glm::vec3(1.0f);
            Log::Push(LogLevel::Warning, "Opened SKL without companion SKN; showing skeleton only.", path.string());
        }

        LoadAnimationsNear(*sklPath, model);
        return model;
    }
    catch (const std::exception& ex)
    {
        errorMessage = ex.what();
        return std::nullopt;
    }
}
}
