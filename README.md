# Vulkan Renderer
- A Vulkan-based 3D real-time renderer using the traditional graphics pipeline (vertex & fragment shaders).
- The renderer is built with C++23, Vulkan 1.4, GLFW for windowing, Volk for Vulkan function loading, Slang for shaders, VMA for memory management, GLM for matrix and vector math, Assimp for model loading, and stb_image for textures.

## Features
- WASD and mouse camera controls
- glTF model and material loading with Assimp
- Texture loading with stb_image
- Blinn-Phong lighting implemented in Slang
- Vulkan 1.4 rendering with dynamic rendering
- Depth testing and back-face culling
- Bindless texture access using descriptor indexing
- Buffer device addresses passed in via push constants
- Alpha-tested transparency on the shader side for textured materials (eg. Cloud's hair)
- Direct texture uploads using Vulkan host image copy
- Synchronization using semaphores, fences, and pipeline barriers

## Video Demo
[![Vulkan Renderer Demo](https://img.youtube.com/vi/bJ2ZzNgNC3E/hqdefault.jpg)](https://youtu.be/bJ2ZzNgNC3E)


## Requirements 
- Compiler supporting C++23
- Vulkan 1.4
- CMake Version 3.7 or greater
- GLFW
- VMA
- GLM
- Assimp


## Build Instructions
1. Go to the root directory
2. Run:
```
cmake -Bbuild && cmake --build build && ./build/main
```

## Attribution
- ["Cloud Strife"](https://sketchfab.com/3d-models/cloud-strife-6d55533956124028864ae3fe0c1d7280) by 雨宮レン ([oscar3dmodel](https://sketchfab.com/oscar3dmodel)), licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
