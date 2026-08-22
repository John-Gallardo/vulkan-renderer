#include "Renderer.h"
#include "Config.h"
#include "Window/Window.h"
#include "volk.h"
#include "vk_mem_alloc.h"
#include <ranges>
#include <utility>
#include <stdexcept>
#include <string_view>
#include <string>
#include <cstdint>
#include <vector>
#include <queue>
#include <print>

void Renderer::initVulkan(Window &window) {
    checkResult(volkInitialize(), "Error: failed to initialize Volk");
    createInstance(window);
    pickPhysicalDevice();
    createDevice();
}

void Renderer::createInstance(Window &window) {
    constexpr VkApplicationInfo applicationInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = Config::title,
        .applicationVersion = Config::version,
        .pEngineName        = nullptr,
        .engineVersion      = {},
        .apiVersion         = VK_API_VERSION_1_4,
    };

    // grab requried instance extensions for GLFW
    auto [glfwExtensionCount, glfwExtensions] = window.getRequiredInstanceExtensions();

    const VkInstanceCreateInfo instanceCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = {},
        .pApplicationInfo        = &applicationInfo,
        .enabledLayerCount       = 0,
        .ppEnabledLayerNames     = nullptr,
        .enabledExtensionCount   = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions
    };

    checkResult(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance), "Error: failed to create Vulkan instance");
    volkLoadInstance(m_instance);
}

void Renderer::pickPhysicalDevice() {
    // enumerate physical devices
    uint32_t physicalDeviceCount{};
    checkResult(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr), "Error: failed to enumerate physical device count");
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    checkResult(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data()), "Error: failed to fill physicalDevices vector");

    // score based on if it's a dGPU or iGPU. we pick dGPU first
    std::priority_queue<std::pair<int, VkPhysicalDevice>> priorityQueue{};
    for (VkPhysicalDevice physicalDevice : physicalDevices) {
        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        
        int score{};
        switch (deviceProperties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score += 2;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score += 1;
                break;
            default:
                continue;
        }
        priorityQueue.push({score, physicalDevice});
    }

    if (!priorityQueue.empty()) {
        m_physicalDevice = priorityQueue.top().second;
    } else {
        throw std::runtime_error("Error: failed to find compatible GPU");
    }

    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    std::println("Selected GPU: {}", deviceProperties.deviceName);
}

void Renderer::createDevice() {
    // enumerate queue families & find first queue family supporting graphics
    uint32_t queueFamilyPropertyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    uint32_t graphicsQueueFamilyIndex{};
    for (auto [i, queueFamilyProperty] : std::views::enumerate(queueFamilyProperties)) {
        if (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamilyIndex = i;
            break;
        }
    }

    float queuePriority{1.0f};  
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = {},
        .queueFamilyIndex = graphicsQueueFamilyIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    // structure chain for our used extensions
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = nullptr,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &vulkan13Features,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &queueCreateInfo,
        .enabledExtensionCount   = static_cast<uint32_t>(m_requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = m_requiredDeviceExtensions.data(),
        .pEnabledFeatures        = nullptr
    };

    checkResult(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device), "Error: failed to create a Vulkan logical device");
    vkGetDeviceQueue(m_device, graphicsQueueFamilyIndex, 0, &m_queue);
    volkLoadDevice(m_device);
}

void Renderer::createAllocator() {
    VmaAllocatorCreateInfo allocatorCreateInfo{
        .flags                          = {},
        .physicalDevice                 = m_physicalDevice,
        .device                         = m_device,
        .preferredLargeHeapBlockSize    = {},       // default = 256MB
        .pAllocationCallbacks           = nullptr,
        .pDeviceMemoryCallbacks         = nullptr,
        .pHeapSizeLimit                 = nullptr,
        .pVulkanFunctions               = {},
        .instance                       = m_instance,
        .vulkanApiVersion               = VK_API_VERSION_1_4,
        .pTypeExternalMemoryHandleTypes = nullptr
    };

    VmaVulkanFunctions vulkanFunctions{};
    checkResult(vmaImportVulkanFunctionsFromVolk(&allocatorCreateInfo, &vulkanFunctions), "Error: failed to import vulkan functions for VMA from Volk");
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    checkResult(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator), "Error: Failed to create VMA Allocator");

}

void Renderer::createSwapchain() {
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
    };

    //vkCreateSwapchainKHR(m_device, &swapchainCreateInfo)
}

void Renderer::checkResult(VkResult result, std::string_view errorMessage) const {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(errorMessage));  // std::string to have it null-terminated
    }
}

void Renderer::cleanup() {
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}
