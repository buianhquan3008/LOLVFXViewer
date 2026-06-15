#pragma once

#include "renderer/Texture2D.h"

#include <filesystem>
#include <optional>

namespace lol
{
class TextureLoader
{
public:
    static std::optional<Texture2D> Load(const std::filesystem::path& path);
};
}
