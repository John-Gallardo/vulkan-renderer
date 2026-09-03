#pragma once
#include "volk.h"
#include <GLFW/glfw3.h>
#include <cstdint>
class Renderer;

struct InstanceExtensionInfo{
    uint32_t     extensionCount{};
    const char **extensions    {};
};

struct FramebufferSize{
    int width{}, height{};
};

struct CursorPosition{
    double x{}, y{};
};

class Window {
public:
    void initWindow();
    bool shouldClose() const;
    bool isKeyPressed(int key) const;
    void closeWindow();
    void pollEvents();
    float getTime();
    void captureCursor();
    void setupMouseCallback(Renderer &renderer);
    InstanceExtensionInfo getRequiredInstanceExtensions() const;
    void createWindowSurface(VkInstance instance, const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);
    FramebufferSize getFramebufferSize();
    void cleanup();
private:
    GLFWwindow *m_window{};
    static void mouseCallback(GLFWwindow *window, double xPos, double yPos);  // NOTE: GLFW expects a non-member function
};
