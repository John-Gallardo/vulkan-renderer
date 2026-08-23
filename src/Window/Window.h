#pragma once
#include "volk.h"
#include <GLFW/glfw3.h>
#include <cstdint>

struct InstanceExtensionInfo{
    uint32_t     extensionCount{};
    const char **extensions    {};
};

struct FramebufferSize{
    int width{}, height{};
};

class Window {
public:
    void initWindow();
    bool shouldClose() const;
    bool isKeyPressed(int key) const;
    void closeWindow();
    void pollEvents();
    InstanceExtensionInfo getRequiredInstanceExtensions() const;
    void createWindowSurface(VkInstance instance, const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);
    FramebufferSize getFramebufferSize();
    void cleanup();
private:
    GLFWwindow *m_window{};
};
