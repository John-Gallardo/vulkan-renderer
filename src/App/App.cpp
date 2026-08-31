#include "App.h"
#include "Window/Window.h"
#include "Input/Input.h"

void App::run() {
    m_window.initWindow();
    m_renderer.initVulkan(m_window);
    m_renderer.loadModels();
    mainLoop();
    cleanup();
}

void App::mainLoop() {
    while (!m_window.shouldClose()) {
        m_window.pollEvents();
        m_input.processUserInput(m_window);
        m_renderer.drawFrame(m_window);
    }
}

void App::cleanup() {
    m_renderer.cleanup();
    m_window.cleanup();
}
