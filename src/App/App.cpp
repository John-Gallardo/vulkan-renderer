#include "App.h"
#include "Window/Window.h"
#include "Input/Input.h"
#include "ModelLoader/ModelLoader.h"

void App::run() {
    m_window.initWindow();
    m_window.captureCursor();
    m_window.setupMouseCallback(m_renderer);
    m_renderer.initVulkan(m_window);
    m_modelLoader.loadModel(m_vertices, m_indices);
    m_renderer.uploadModel(m_vertices, m_indices);
    mainLoop();
    cleanup();
}

void App::mainLoop() {
    while (!m_window.shouldClose()) {
        float currentFrame{m_window.getTime()};
        m_deltaTime = currentFrame - m_lastFrame;
        m_lastFrame = currentFrame;

        m_window.pollEvents();
        m_input.processUserInput(m_window);
        m_renderer.processCameraMovement(m_window, m_deltaTime);
        m_renderer.drawFrame(m_window);
    }
}

void App::cleanup() {
    m_renderer.cleanup();
    m_window.cleanup();
}
