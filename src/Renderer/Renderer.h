#pragma once
#include "volk.h"
#include "vk_mem_alloc.h"
#include <vector>
#include <cstdint>
#include <string>
class Window;

class Renderer {
public:
    void initVulkan(Window &window);
    void drawFrame();
    void cleanup();
private:
    VkInstance                   m_instance                {VK_NULL_HANDLE};
    VkPhysicalDevice             m_physicalDevice          {VK_NULL_HANDLE};
    VkDevice                     m_device                  {VK_NULL_HANDLE};
    VkQueue                      m_queue                   {VK_NULL_HANDLE};
    uint32_t                     m_queueIndex              {};
    VmaAllocator                 m_allocator               {VK_NULL_HANDLE};
    VkSurfaceKHR                 m_surface                 {VK_NULL_HANDLE};
    VkSwapchainKHR               m_swapchain               {VK_NULL_HANDLE};
    VkExtent2D                   m_swapchainExtent         {};
    VkSurfaceFormatKHR           m_swapchainSurfaceFormat  {};
    std::vector<VkImage>         m_swapchainImages         {};
    std::vector<VkImageView>     m_swapchainImageViews     {};
    VkPipelineLayout             m_pipelineLayout          {VK_NULL_HANDLE};
    VkPipeline                   m_graphicsPipeline        {VK_NULL_HANDLE};
    VkCommandPool                m_commandPool             {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers          {};
    std::vector<VkFence>         m_renderWaitFences        {};
    std::vector<VkSemaphore>     m_acquireImageSemaphores  {};
    std::vector<VkSemaphore>     m_renderFinishedSemaphores{};
    uint32_t                     m_frameIndex              {};
    std::vector<const char *>    m_requiredDeviceExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
        VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME
    };

    void createInstance(Window &window);
    void pickPhysicalDevice();
    void createDevice();
    void createAllocator();
    void createSurface(Window &window);
    void createSwapchain(Window &window);
    void createImageViews();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // Helper functions
    std::vector<char> readFile(const std::string &filename);
    void checkResult(VkResult result, const char *errorMessage) const;
    void swapchainCleanup();
    void syncObjectCleanup();
};
