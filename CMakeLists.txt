cmake_minimum_required(VERSION 3.24)

project(LoLAssetStudio VERSION 0.1.0 LANGUAGES C CXX)

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
option(LOL_ASSET_STUDIO_USE_SYSTEM_DEPS "Use installed/system dependencies instead of FetchContent where possible" OFF)

set(LOL_ASSET_STUDIO_GLFW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw" CACHE PATH "Local GLFW source root")
set(LOL_ASSET_STUDIO_GLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/glm" CACHE PATH "Local GLM source root")
set(LOL_ASSET_STUDIO_STB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/stb" CACHE PATH "Local stb source root")
set(LOL_ASSET_STUDIO_JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/json" CACHE PATH "Local nlohmann/json source root")
set(LOL_ASSET_STUDIO_ASSIMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/assimp" CACHE PATH "Local Assimp source root")
set(LOL_ASSET_STUDIO_IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui" CACHE PATH "Local ImGui source root")

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

function(lol_asset_studio_declare_fetchcontent name local_root repo tag)
  if(EXISTS "${local_root}")
    FetchContent_Declare(${name} SOURCE_DIR "${local_root}")
  else()
    FetchContent_Declare(
      ${name}
      GIT_REPOSITORY ${repo}
      GIT_TAG ${tag}
    )
  endif()
endfunction()

function(lol_asset_studio_pick_target out_var)
  foreach(candidate IN ITEMS ${ARGN})
    if(TARGET "${candidate}")
      set(${out_var} "${candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  string(REPLACE ";" ", " formatted_candidates "${ARGN}")
  message(FATAL_ERROR "Could not find any of the expected CMake targets: ${formatted_candidates}")
endfunction()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(LOL_ASSET_STUDIO_USE_SYSTEM_DEPS)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(glm CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  find_package(assimp CONFIG REQUIRED)

  find_path(STB_INCLUDE_DIR
    NAMES stb_image.h
    HINTS "${LOL_ASSET_STUDIO_STB_ROOT}"
    PATH_SUFFIXES include
    REQUIRED
  )

  find_path(IMGUI_SOURCE_DIR
    NAMES imgui.cpp
    HINTS "${LOL_ASSET_STUDIO_IMGUI_ROOT}"
    REQUIRED
  )

  if(NOT EXISTS "${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
    message(FATAL_ERROR "ImGui backends were not found under ${IMGUI_SOURCE_DIR}. Point LOL_ASSET_STUDIO_IMGUI_ROOT at an ImGui source tree that contains the backends folder.")
  endif()

  set(imgui_SOURCE_DIR "${IMGUI_SOURCE_DIR}")
  lol_asset_studio_pick_target(LOL_ASSET_STUDIO_GLFW_TARGET glfw glfw3 glfw::glfw)
  lol_asset_studio_pick_target(LOL_ASSET_STUDIO_ASSIMP_TARGET assimp assimp::assimp)
else()
  lol_asset_studio_declare_fetchcontent(glfw "${LOL_ASSET_STUDIO_GLFW_ROOT}" https://github.com/glfw/glfw.git 3.4)
  lol_asset_studio_declare_fetchcontent(glm "${LOL_ASSET_STUDIO_GLM_ROOT}" https://github.com/g-truc/glm.git 1.0.1)
  lol_asset_studio_declare_fetchcontent(stb "${LOL_ASSET_STUDIO_STB_ROOT}" https://github.com/nothings/stb.git master)
  lol_asset_studio_declare_fetchcontent(json "${LOL_ASSET_STUDIO_JSON_ROOT}" https://github.com/nlohmann/json.git v3.12.0)
  lol_asset_studio_declare_fetchcontent(assimp "${LOL_ASSET_STUDIO_ASSIMP_ROOT}" https://github.com/assimp/assimp.git v5.4.3)
  lol_asset_studio_declare_fetchcontent(imgui "${LOL_ASSET_STUDIO_IMGUI_ROOT}" https://github.com/ocornut/imgui.git docking)

  FetchContent_MakeAvailable(glfw glm json assimp)

  FetchContent_GetProperties(stb)
  if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
  endif()

  FetchContent_GetProperties(imgui)
  if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
  endif()

  set(STB_INCLUDE_DIR "${stb_SOURCE_DIR}")
  set(LOL_ASSET_STUDIO_GLFW_TARGET glfw)
  set(LOL_ASSET_STUDIO_ASSIMP_TARGET assimp)
endif()

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

target_link_libraries(imgui_lib PUBLIC ${LOL_ASSET_STUDIO_GLFW_TARGET})

file(GLOB_RECURSE LOL_ASSET_STUDIO_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
)
list(FILTER LOL_ASSET_STUDIO_SOURCES EXCLUDE REGEX ".*/src/tests/.*\\.cpp$")

add_executable(LoLAssetStudio ${LOL_ASSET_STUDIO_SOURCES})

target_include_directories(LoLAssetStudio
  PRIVATE
    src
    external/glad/include
    ${STB_INCLUDE_DIR}
)

target_link_libraries(LoLAssetStudio
  PRIVATE
    ${LOL_ASSET_STUDIO_ASSIMP_TARGET}
    glad_api
    ${LOL_ASSET_STUDIO_GLFW_TARGET}
    glm::glm
    imgui_lib
    nlohmann_json::nlohmann_json
)

if(WIN32)
  target_link_libraries(LoLAssetStudio PRIVATE opengl32)
endif()

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
  add_executable(LoLAssetStudioTests
    src/tests/PlaceholderTests.cpp
    src/bin/BinToVfxGraph.cpp
    src/bin/BinTokenizer.cpp
    src/bin/BinParser.cpp
  )
  target_include_directories(LoLAssetStudioTests PRIVATE src)
  target_link_libraries(LoLAssetStudioTests PRIVATE glm::glm nlohmann_json::nlohmann_json)
  add_test(NAME LoLAssetStudioTests COMMAND LoLAssetStudioTests)
endif()
