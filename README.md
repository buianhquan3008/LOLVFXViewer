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
- Visual Studio 2022/2026 with Desktop C++ workload
- CMake 3.24 or newer
- Git

The project uses `FetchContent`, so third-party dependencies are downloaded during configure. Generated GLAD sources are vendored in `external/glad`, so Python is not required for normal builds.

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
