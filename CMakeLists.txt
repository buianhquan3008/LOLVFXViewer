cmake_minimum_required(VERSION 3.24)

project(LoLAssetStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)

option(LOL_ASSET_STUDIO_BUILD_TESTS "Build tests" ON)
option(LOL_ASSET_STUDIO_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)

FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.4.3
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glm stb json assimp imgui)

add_library(glad_api STATIC external/glad/src/gl.c)
target_include_directories(glad_api PUBLIC external/glad/include)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC glfw)

file(GLOB_RECURSE LOL_ASSET_STUDIO_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
)
list(FILTER LOL_ASSET_STUDIO_SOURCES EXCLUDE REGEX ".*/src/tests/.*\\.cpp$")

add_executable(LoLAssetStudio ${LOL_ASSET_STUDIO_SOURCES})

target_include_directories(LoLAssetStudio
  PRIVATE
    src
    external/glad/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(LoLAssetStudio
  PRIVATE
    assimp
    glad_api
    glfw
    glm::glm
    imgui_lib
    nlohmann_json::nlohmann_json
)

target_compile_definitions(LoLAssetStudio PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
  target_compile_options(LoLAssetStudio PRIVATE /W4 /permissive- /Zc:__cplusplus)
  if(LOL_ASSET_STUDIO_WARNINGS_AS_ERRORS)
    target_compile_options(LoLAssetStudio PRIVATE /WX)
  endif()
else()
  target_compile_options(LoLAssetStudio PRIVATE -Wall -Wextra -Wpedantic)
  if(LOL_ASSET_STUDIO_WARNINGS_AS_ERRORS)
    target_compile_options(LoLAssetStudio PRIVATE -Werror)
  endif()
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${LOL_ASSET_STUDIO_SOURCES})

if(LOL_ASSET_STUDIO_BUILD_TESTS)
  enable_testing()
  add_executable(LoLAssetStudioTests src/tests/PlaceholderTests.cpp)
  target_include_directories(LoLAssetStudioTests PRIVATE src)
  target_link_libraries(LoLAssetStudioTests PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME LoLAssetStudioTests COMMAND LoLAssetStudioTests)
endif()
