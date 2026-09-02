#include "ModelLoader.h"
#include <stdexcept>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>      
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <ranges>
#include <cstdint>
#include <format>
#include <glm/vec3.hpp>  // IWYU pragma: keep
#include <glm/vec2.hpp>  // IWYU pragma: keep
#include <string>                  
#include <vector>                  

void ModelLoader::loadModel(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {
    // 1. load model using assimp
    Assimp::Importer importer{};
    const char *path{PROJECT_ROOT_DIR "models/cloud_strife/scene.gltf"};
    const aiScene *scene{importer.ReadFile(
        path,
        aiProcess_Triangulate |          // make sure all faces are triangles
        aiProcess_JoinIdenticalVertices  // merge duplicate vertices
    )};

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error{std::format("Assimp error: {}\n", importer.GetErrorString())};
        throw std::runtime_error(error);
    }

    // 2. load entire model into a single vertex and index buffer
    
    for (uint32_t i : std::views::iota(0u, scene->mNumMeshes)) {
        const aiMesh *mesh{scene->mMeshes[i]};
        
        uint32_t vertexOffset{static_cast<uint32_t>(vertices.size())};  // store offset BEFORE adding this mesh's new vertices
        for (uint32_t j : std::views::iota(0u, mesh->mNumVertices)) {
            Vertex vertex{};
            vertex.position = {
                mesh->mVertices[j].x,
                mesh->mVertices[j].y,
                mesh->mVertices[j].z
            };

            if (mesh->HasNormals()) {
                vertex.normal = {
                    mesh->mNormals[j].x,
                    mesh->mNormals[j].y,
                    mesh->mNormals[j].z
                };
            }

            // TODO: fill texture later

            vertices.push_back(vertex);
        }

        for (uint32_t j : std::views::iota(0u, mesh->mNumFaces)) {
            const aiFace &face{mesh->mFaces[j]};
            for (uint32_t k : std::views::iota(0u, face.mNumIndices)) {
                indices.push_back(vertexOffset + face.mIndices[k]);
            }
        }

    }

}
