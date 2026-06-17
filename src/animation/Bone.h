#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

namespace lol
{
struct Bone
{
    std::string name;
    int parentIndex = -1;
    bool isBone = false;
    glm::mat4 localBindTransform = glm::mat4(1.0f);
    glm::vec3 bindPosition = glm::vec3(0.0f);
    glm::quat bindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 bindScale = glm::vec3(1.0f);
    glm::mat4 globalBindTransform = glm::mat4(1.0f);
    glm::mat4 inverseBindTransform = glm::mat4(1.0f);
};
}
