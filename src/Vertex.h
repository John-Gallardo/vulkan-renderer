#pragma once
#include <glm/glm.hpp>
#include <cstdint>

struct Vertex{
    glm::vec3 position    {};
    glm::vec3 normal      {};
    glm::vec2 texCoords   {};
    uint32_t materialIndex{};
};
