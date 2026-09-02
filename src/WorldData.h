#pragma once
#include <glm/glm.hpp>

struct WorldData{
    glm::mat4 model     {};
    glm::mat4 view      {};
    glm::mat4 projection{};
};
