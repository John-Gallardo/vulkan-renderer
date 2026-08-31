#include "ModelLoader.h"
#include <stdexcept>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>      
#include <assimp/postprocess.h>
#include <format>

void ModelLoader::loadModels() {
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

    // 2. load vertices and indices
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    
}
