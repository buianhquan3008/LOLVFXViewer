#pragma once

#include "vfx/VfxGraph.h"

#include <filesystem>
#include <optional>
#include <string>

namespace lol
{
class VfxLoader
{
public:
    static bool IsSupportedPath(const std::filesystem::path& path);
    static std::optional<VfxGraph> Load(const std::filesystem::path& path, std::string& warnings);
};
}
