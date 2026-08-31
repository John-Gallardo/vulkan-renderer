#pragma once
#include <glm/detail/qualifier.hpp>   // for qualifier
#include <glm/ext/vector_float2.hpp>  // for vec2
#include <glm/ext/vector_float3.hpp>  // for vec3
#include <string>
#include <vector>
#include <cstdint>

struct Vertex{
    glm::vec3 Position {};
    glm::vec3 Normal   {};
    glm::vec2 TexCoords{};
};

struct Texture{
    unsigned int id {};
    std::string type{};  // diffuse, specular, etc...
};

class Mesh{
public:
    std::vector<Vertex>   vertices{};
    std::vector<uint32_t> indices{};
    std::vector<Texture>  textures{};
    
    Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Texture> textures)
        : vertices{vertices}, indices{indices}, textures{textures}
    {}
};

class ModelLoader{
public:
    void loadModels();
private:
};
