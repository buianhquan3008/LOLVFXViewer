#pragma once

#include "assets/ModelLoader.h"

#include <filesystem>
#include <optional>
#include <string>

namespace lol
{
class RiotModelLoader
{
public:
    static bool IsRiotModelPath(const std::filesystem::path& path);
    static std::optional<ModelData> Load(const std::filesystem::path& path, std::string& errorMessage);
};
}
