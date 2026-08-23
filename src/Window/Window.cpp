#include "volk.h"
#include "Window.h"
#include "Config.h"
#include <stdexcept>
#include <GLFW/glfw3.h>

void Window::initWindow() {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Error: Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(Config::width, Config::height, Config::title, nullptr, nullptr);

    if (m_window == nullptr) {
        throw std::runtime_error("Error: Failed to create GLFW window");
    }
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

bool Window::isKeyPressed(int key) const {
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

void Window::closeWindow() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Window::pollEvents() {
    glfwPollEvents();
}

InstanceExtensionInfo Window::getRequiredInstanceExtensions() const {
    uint32_t glfwExtensionCount{};
    const char **glfwExtensions{glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};
    return {glfwExtensionCount, glfwExtensions};
}

void Window::createWindowSurface(VkInstance instance, const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, m_window, allocator, surface) != VK_SUCCESS) {
        throw std::runtime_error("Error: Failed to create Vulkan window surface");
    }
}

FramebufferSize Window::getFramebufferSize() {
    int width{}, height{};
    glfwGetFramebufferSize(m_window, &width, &height);
    return {width, height};
}

void Window::cleanup() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
