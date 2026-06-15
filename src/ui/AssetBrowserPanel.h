#pragma once

#include "ui/AppState.h"

namespace lol
{
class AssetBrowserPanel
{
public:
    void Draw(AppState& state);

private:
    void DrawDirectoryTree(AppState& state, const std::filesystem::path& path);
};
}
