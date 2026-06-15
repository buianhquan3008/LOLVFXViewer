#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace lol
{
struct VfxMaterialRef
{
    std::string name;
    std::string texturePath;
};

struct VfxCurve
{
    std::string name;
    std::vector<glm::vec2> points;
};

struct VfxChildEffect
{
    std::string name;
};

struct VfxEmitter
{
    std::string name;
    float spawnRate = 0.0f;
    float lifetimeMin = 1.0f;
    float lifetimeMax = 1.0f;
    glm::vec3 velocityMin = glm::vec3(0.0f);
    glm::vec3 velocityMax = glm::vec3(0.0f);
    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    float startSize = 1.0f;
    float endSize = 0.0f;
    std::string texturePath;
    std::string materialName;
};

struct VfxGraph
{
    std::string name;
    std::vector<VfxEmitter> emitters;
    std::vector<VfxMaterialRef> materials;
    std::vector<VfxCurve> curves;
    std::vector<VfxChildEffect> children;
};
}
