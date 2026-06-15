#include "assets/AssetManager.h"

#include "assets/TextureLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace lol
{
namespace
{
std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

void AssetManager::SetRoot(const std::filesystem::path& root)
{
    m_root = root;
}

std::vector<std::filesystem::directory_entry> AssetManager::EnumerateDirectory(const std::filesystem::path& directory) const
{
    std::vector<std::filesystem::directory_entry> entries;
    if (!std::filesystem::exists(directory))
    {
        return entries;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        entries.push_back(entry);
    }

    std::ranges::sort(entries, [](const auto& lhs, const auto& rhs) {
        if (lhs.is_directory() != rhs.is_directory())
        {
            return lhs.is_directory() > rhs.is_directory();
        }
        return lhs.path().filename().string() < rhs.path().filename().string();
    });

    return entries;
}

bool AssetManager::IsSupportedTexture(const std::filesystem::path& path) const
{
    const auto ext = Lowercase(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga";
}

bool AssetManager::IsSupportedModel(const std::filesystem::path& path) const
{
    const auto ext = Lowercase(path.extension().string());
    return ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb";
}

bool AssetManager::IsDataFile(const std::filesystem::path& path) const
{
    const auto value = Lowercase(path.filename().string());
    return Lowercase(path.extension().string()) == ".json" || value.ends_with(".bin.txt");
}

AssetSelection AssetManager::Open(const std::filesystem::path& path)
{
    AssetSelection selection;
    selection.path = path;

    if (IsSupportedTexture(path))
    {
        selection.type = AssetSelectionType::Texture;
        selection.texture = TextureLoader::Load(path);
        if (!selection.texture)
        {
            selection.type = AssetSelectionType::Unsupported;
            selection.metadata = "Failed to load texture.";
        }
        return selection;
    }

    if (IsSupportedModel(path))
    {
        selection.type = AssetSelectionType::Model;
        std::string errorMessage;
        selection.model = ModelLoader::Load(path, errorMessage);
        if (!selection.model)
        {
            selection.type = AssetSelectionType::Unsupported;
            selection.metadata = errorMessage;
        }
        return selection;
    }

    if (IsDataFile(path))
    {
        selection.type = AssetSelectionType::Data;
        std::ifstream stream(path);
        std::stringstream buffer;
        buffer << stream.rdbuf();
        selection.metadata = buffer.str();
        return selection;
    }

    selection.type = AssetSelectionType::Unsupported;
    selection.metadata = "Unsupported asset type.";
    return selection;
}
}
