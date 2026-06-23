# LoL Asset Studio

LoL Asset Studio is a Windows desktop application for previewing extracted League of Legends assets outside the game client. The current codebase implements the Phase 1 MVP shell: asset browsing, image preview, an OpenGL viewport, basic Assimp model loading, and placeholder interfaces for later animation, BIN parsing, and VFX work.

## Scope

Supported in this repository:

- Local extracted asset folders
- Texture preview for `.png`, `.jpg`, `.jpeg`, `.tga`
- Basic model preview for Assimp-supported formats like `.obj`, `.fbx`, `.gltf`, `.glb`
- Desktop-only developer tooling

Explicitly out of scope:

- Game injection
- Memory editing
- Anti-cheat bypass
- Live client patching
- Gameplay automation

## Build

Requirements:

- Windows 10 or 11
- CMake 3.24 or newer
- Git
- One of:
  - Visual Studio 2022/2026 with Desktop C++ workload
  - MinGW-w64 + Ninja

Generated GLAD sources are vendored in `external/glad`, so Python is not required for normal builds.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

If `cmake` is not on your `PATH`, use the copy bundled with Visual Studio, for example:

```powershell
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -G "Visual Studio 18 2026" -A x64
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -G "Visual Studio 18 2026" -A x64
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
```

### MinGW / Ninja on this machine

This repository now supports two practical Windows + MinGW flows:

1. `system deps`: use dependencies installed on the machine
2. `local deps`: point CMake at unpacked dependency source trees in `external/`

#### Option 1: MinGW + Ninja with system dependencies

Use this when `glfw`, `glm`, `nlohmann_json`, and `assimp` are already installed with CMake package files, and when `stb` and `imgui` sources are available locally.

Expected local folders by default:

- `external/stb`
- `external/imgui`

Configure and build:

```powershell
cmake --preset mingw-ninja-system
cmake --build --preset mingw-ninja-system
```

If your local `stb` or `imgui` trees live elsewhere, override them:

```powershell
cmake -S . -B build/mingw-system -G Ninja `
  -DLOL_ASSET_STUDIO_USE_SYSTEM_DEPS=ON `
  -DLOL_ASSET_STUDIO_STB_ROOT=D:/deps/stb `
  -DLOL_ASSET_STUDIO_IMGUI_ROOT=D:/deps/imgui
cmake --build build/mingw-system
```

#### Option 2: MinGW + Ninja with local vendored sources

Use this when the machine cannot download from GitHub during configure. Put dependency source trees in these folders:

- `external/glfw`
- `external/glm`
- `external/stb`
- `external/json`
- `external/assimp`
- `external/imgui`

Then build:

```powershell
cmake --preset mingw-ninja-local
cmake --build --preset mingw-ninja-local
```

You can also point each dependency at a different local folder via CMake cache variables:

- `LOL_ASSET_STUDIO_GLFW_ROOT`
- `LOL_ASSET_STUDIO_GLM_ROOT`
- `LOL_ASSET_STUDIO_STB_ROOT`
- `LOL_ASSET_STUDIO_JSON_ROOT`
- `LOL_ASSET_STUDIO_ASSIMP_ROOT`
- `LOL_ASSET_STUDIO_IMGUI_ROOT`

#### Current machine status

The current environment has `g++` and `ninja`, but it does not currently have the required third-party packages installed, and it cannot fetch them from GitHub during configure. Because of that, the project still will not build until you provide either:

- installed system packages with CMake config files, or
- local dependency source trees under `external/` or custom `LOL_ASSET_STUDIO_*_ROOT` paths

Once those dependencies are present, the MinGW presets above are the intended way to build and run on this machine.

## Current MVP

- GLFW + GLAD + OpenGL 4.6 setup
- ImGui docking layout with menu, asset browser, viewport, inspector, and console
- `std::filesystem` asset browser with filtering and search
- Texture preview backed by `stb_image`
- Model import via Assimp with internal mesh conversion
- Orbit camera and simple grid renderer
- Lightweight logging system
- Placeholder interfaces for BIN, animation, and VFX phases

## Notes

- Riot-specific loaders are intentionally left as TODO placeholders.
- The viewport is focused on clean architecture and iteration speed for Phase 1, not on final rendering fidelity.

## Documentation

- [Riot SKN/SKL/ANM implementation notes](docs/riot-skn-skl-anm-implementation.md)
