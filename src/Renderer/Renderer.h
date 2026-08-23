#pragma once
#include "volk.h"
#include "vk_mem_alloc.h"
#include <string>
#include <vector>
class Window;

class Renderer {
public:
    void initVulkan(Window &window);
    void cleanup();
private:
    VkInstance                m_instance                {VK_NULL_HANDLE};
    VkPhysicalDevice          m_physicalDevice          {VK_NULL_HANDLE};
    VkDevice                  m_device                  {VK_NULL_HANDLE};
    VkQueue                   m_queue                   {VK_NULL_HANDLE};
    VmaAllocator              m_allocator               {VK_NULL_HANDLE};
    VkSurfaceKHR              m_surface                 {VK_NULL_HANDLE};
    VkSwapchainKHR            m_swapchain               {VK_NULL_HANDLE};
    std::vector<const char *> m_requiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    void createInstance(Window &window);
    void pickPhysicalDevice();
    void createDevice();
    void createAllocator();
    void createSurface(Window &window);
    void createSwapchain(Window &window);
    void checkResult(VkResult result, std::string errorMessage) const;
};
