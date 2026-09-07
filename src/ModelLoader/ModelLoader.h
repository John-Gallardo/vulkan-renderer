// NOTE: IWYU false positives on ModelLoader.h and ModelLoader.cpp...
#pragma once
#include <glm/vec3.hpp>  // IWYU pragma: keep
#include <glm/vec2.hpp>  // IWYU pragma: keep
#include <vector>
#include <cstdint>
struct Vertex;
struct Material;
class Renderer;

class ModelLoader{
public:
    void loadModel(
        std::vector<Vertex> &vertices,
        std::vector<uint32_t> &indices,
        std::vector<Material> &materials,
        Renderer &renderer
    );
private:
};
