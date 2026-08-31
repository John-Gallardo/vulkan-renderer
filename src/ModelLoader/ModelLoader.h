// NOTE: IWYU false positives on ModelLoader.h and ModelLoader.cpp...
#pragma once
#include <glm/vec3.hpp>  // IWYU pragma: keep
#include <glm/vec2.hpp>  // IWYU pragma: keep
#include <vector>
#include <cstdint>

struct Vertex{
    glm::vec3 Position {};
    glm::vec3 Normal   {};
    glm::vec2 TexCoords{};
};

class ModelLoader{
public:
    void loadModel(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);
private:
};
