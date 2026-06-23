#include "vfx/VfxLoader.h"

#include "bin/BinParser.h"
#include "bin/BinToVfxGraph.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace lol
{
namespace
{
using json = nlohmann::json;

std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool Contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

glm::vec3 JsonVec3(const json& value, const glm::vec3& fallback)
{
    if (!value.is_array() || value.size() < 3)
    {
        return fallback;
    }

    return glm::vec3(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>()
    );
}

glm::vec4 JsonVec4(const json& value, const glm::vec4& fallback)
{
    if (!value.is_array() || value.size() < 4)
    {
        return fallback;
    }

    return glm::vec4(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>(),
        value[3].get<float>()
    );
}

std::string ResolveRelativePath(const std::filesystem::path& sourcePath, const std::string& candidate)
{
    if (candidate.empty())
    {
        return {};
    }

    std::filesystem::path resolved(candidate);
    if (resolved.is_absolute() && std::filesystem::exists(resolved))
    {
        return resolved.lexically_normal().string();
    }

    resolved = sourcePath.parent_path() / resolved;
    if (std::filesystem::exists(resolved))
    {
        return resolved.lexically_normal().string();
    }

    return candidate;
}

VfxEmitter DefaultEmitter()
{
    VfxEmitter emitter;
    emitter.name = "Default";
    emitter.spawnRate = 16.0f;
    emitter.lifetimeMin = 0.7f;
    emitter.lifetimeMax = 1.2f;
    emitter.velocityMin = glm::vec3(-0.2f, 0.8f, -0.2f);
    emitter.velocityMax = glm::vec3(0.2f, 1.5f, 0.2f);
    emitter.startColor = glm::vec4(1.0f, 0.75f, 0.2f, 1.0f);
    emitter.endColor = glm::vec4(1.0f, 0.1f, 0.0f, 0.0f);
    emitter.startSize = 0.35f;
    emitter.endSize = 0.08f;
    emitter.maxParticles = 256;
    emitter.additiveBlending = true;
    return emitter;
}

std::optional<VfxGraph> LoadJsonGraph(const std::filesystem::path& path, const std::string& text)
{
    json root = json::parse(text, nullptr, true, true);
    VfxGraph graph;
    graph.name = root.value("name", path.stem().string());
    graph.durationSeconds = root.value("durationSeconds", 5.0f);
    graph.loop = root.value("loop", true);

    if (root.contains("materials") && root["materials"].is_array())
    {
        for (const auto& materialValue : root["materials"])
        {
            VfxMaterialRef material;
            material.name = materialValue.value("name", "");
            material.texturePath = ResolveRelativePath(path, materialValue.value("texturePath", ""));
            if (materialValue.contains("tint"))
            {
                material.tint = JsonVec4(materialValue["tint"], material.tint);
            }
            graph.materials.push_back(std::move(material));
        }
    }

    if (root.contains("emitters") && root["emitters"].is_array())
    {
        for (const auto& emitterValue : root["emitters"])
        {
            VfxEmitter emitter = DefaultEmitter();
            emitter.name = emitterValue.value("name", emitter.name);
            emitter.spawnRate = emitterValue.value("spawnRate", emitter.spawnRate);
            emitter.lifetimeMin = emitterValue.value("lifetimeMin", emitter.lifetimeMin);
            emitter.lifetimeMax = emitterValue.value("lifetimeMax", emitter.lifetimeMax);
            emitter.startSize = emitterValue.value("startSize", emitter.startSize);
            emitter.endSize = emitterValue.value("endSize", emitter.endSize);
            emitter.burstCount = emitterValue.value("burstCount", emitter.burstCount);
            emitter.maxParticles = emitterValue.value("maxParticles", emitter.maxParticles);
            emitter.loop = emitterValue.value("loop", emitter.loop);
            emitter.additiveBlending = emitterValue.value("additiveBlending", emitter.additiveBlending);
            emitter.billboard = emitterValue.value("billboard", emitter.billboard);
            emitter.materialName = emitterValue.value("materialName", "");
            emitter.texturePath = ResolveRelativePath(path, emitterValue.value("texturePath", ""));
            if (emitterValue.contains("velocityMin"))
            {
                emitter.velocityMin = JsonVec3(emitterValue["velocityMin"], emitter.velocityMin);
            }
            if (emitterValue.contains("velocityMax"))
            {
                emitter.velocityMax = JsonVec3(emitterValue["velocityMax"], emitter.velocityMax);
            }
            if (emitterValue.contains("startColor"))
            {
                emitter.startColor = JsonVec4(emitterValue["startColor"], emitter.startColor);
            }
            if (emitterValue.contains("endColor"))
            {
                emitter.endColor = JsonVec4(emitterValue["endColor"], emitter.endColor);
            }
            graph.emitters.push_back(std::move(emitter));
        }
    }

    if (root.contains("children") && root["children"].is_array())
    {
        for (const auto& childValue : root["children"])
        {
            VfxChildEffect child;
            child.name = childValue.value("name", "");
            child.path = ResolveRelativePath(path, childValue.value("path", ""));
            graph.children.push_back(std::move(child));
        }
    }

    return graph;
}
}

bool VfxLoader::IsSupportedPath(const std::filesystem::path& path)
{
    const std::string extension = Lowercase(path.extension().string());
    const std::string filename = Lowercase(path.filename().string());
    if (extension == ".json" && Contains(filename, "vfx"))
    {
        return true;
    }

    return filename.ends_with(".bin.txt") || filename.ends_with(".vfx.txt");
}

std::optional<VfxGraph> VfxLoader::Load(const std::filesystem::path& path, std::string& warnings)
{
    warnings.clear();

    std::ifstream probe(path);
    if (!probe)
    {
        warnings = "Failed to open VFX file.";
        return std::nullopt;
    }

    const std::string text = ReadAllText(path);
    try
    {
        if (Lowercase(path.extension().string()) == ".json")
        {
            return LoadJsonGraph(path, text);
        }

        BinParser parser;
        BinObject root;
        BinParseError error;
        if (!parser.Parse(text, root, error))
        {
            warnings = "BIN parse error at " + std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
                ": " + error.message;
            return std::nullopt;
        }

        return BinToVfxGraph::Convert(root, path, warnings);
    }
    catch (const std::exception& exception)
    {
        warnings += std::string("VFX parse error: ") + exception.what();
        return std::nullopt;
    }
}
}
