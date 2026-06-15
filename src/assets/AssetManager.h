#pragma once

#include "assets/ModelLoader.h"
#include "renderer/Texture2D.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lol
{
enum class AssetSelectionType
{
    None,
    Texture,
    Model,
    Data,
    Unsupported
};

struct AssetSelection
{
    AssetSelectionType type = AssetSelectionType::None;
    std::filesystem::path path;
    std::optional<Texture2D> texture;
    std::optional<ModelData> model;
    std::string metadata;
};

class AssetManager
{
public:
    void SetRoot(const std::filesystem::path& root);
    const std::filesystem::path& Root() const { return m_root; }

    std::vector<std::filesystem::directory_entry> EnumerateDirectory(const std::filesystem::path& directory) const;
    bool IsSupportedTexture(const std::filesystem::path& path) const;
    bool IsSupportedModel(const std::filesystem::path& path) const;
    bool IsDataFile(const std::filesystem::path& path) const;
    AssetSelection Open(const std::filesystem::path& path);

private:
    std::filesystem::path m_root;
};
}
