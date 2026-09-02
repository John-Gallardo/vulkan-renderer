#pragma once
#include "Renderer/Renderer.h"
#include "Window/Window.h"
#include "Input/Input.h"
#include "ModelLoader/ModelLoader.h"
#include "Vertex.h"
#include <cstdint>
#include <vector>

class App {
public:
    void run();

private:
    Window                m_window{};
    Input                 m_input{};
    Renderer              m_renderer{};
    ModelLoader           m_modelLoader{};
    std::vector<Vertex>   m_vertices{};
    std::vector<uint32_t> m_indices{};

    void mainLoop();
    void cleanup();
};
