#pragma once

#include <glm/glm.hpp>

namespace lol
{
struct Particle
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    float startSize = 1.0f;
    float endSize = 0.0f;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 1.0f;
};

struct ParticleRenderData
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec4 color = glm::vec4(1.0f);
    float size = 1.0f;
};
}
