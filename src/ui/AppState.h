#pragma once

#include "animation/Animator.h"
#include "assets/AssetManager.h"
#include "renderer/Camera.h"
#include "renderer/Renderer.h"
#include "vfx/VfxRuntime.h"

#include <filesystem>
#include <string>

namespace lol
{
struct AppState
{
    AssetManager assetManager;
    AssetSelection selection;
    OrbitCamera camera;
    ViewportOptions viewportOptions;
    std::filesystem::path currentDirectory;
    std::string searchText;
    std::string rootPathInput;
    bool viewportHovered = false;
    bool viewportFocused = false;
    bool pauseAnimation = false;
    bool enableSkinning = true;
    bool pauseVfx = false;
    bool enableVfx = true;
    int selectedBoneIndex = -1;
    Animator animator;
    VfxRuntime vfxRuntime;
};
}
