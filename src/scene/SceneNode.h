#pragma once

#include "scene/Transform.h"

#include <memory>
#include <string>
#include <vector>

namespace lol
{
class SceneNode
{
public:
    Transform localTransform;
    std::string name;
    std::vector<std::unique_ptr<SceneNode>> children;
};
}
