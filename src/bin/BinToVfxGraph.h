#pragma once

#include "bin/BinAst.h"
#include "vfx/VfxGraph.h"

#include <filesystem>
#include <optional>
#include <string>

namespace lol
{
class BinToVfxGraph
{
public:
    static std::optional<VfxGraph> Convert(const BinObject& root, const std::filesystem::path& sourcePath, std::string& warnings);
};
}
