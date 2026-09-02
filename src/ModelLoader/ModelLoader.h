// NOTE: IWYU false positives on ModelLoader.h and ModelLoader.cpp...
#pragma once
#include "Vertex.h"
#include <glm/vec3.hpp>  // IWYU pragma: keep
#include <glm/vec2.hpp>  // IWYU pragma: keep
#include <vector>
#include <cstdint>

class ModelLoader{
public:
    void loadModel(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);
private:
};
