#include "Renderer.h"
#include "Config.h"
#include "Window/Window.h"
#include "volk.h"
#include "vk_mem_alloc.h"
#include <ranges>
#include <utility>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <vector>
#include <queue>
#include <print>

void Renderer::initVulkan(Window &window) {
    checkResult(volkInitialize(), "Error: failed to initialize Volk");
    createInstance(window);
    pickPhysicalDevice();
    createDevice();
    createAllocator();
    createSurface(window);
    createSwapchain(window);
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

void Renderer::createSurface(Window &window) {
    window.createWindowSurface(m_instance, nullptr, &m_surface);
}

void Renderer::createSwapchain(Window &window) {
    // Grab surface capabilities, minImageCount, imageExtent
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);  // NOTE: not using version 2 since I don't need the extra features

    // min image count
    uint32_t minImageCount{std::max(3u, surfaceCapabilities.minImageCount)};
    // clamp if we went over max
    bool hasMaximum{0 != surfaceCapabilities.maxImageCount};
    if (hasMaximum && surfaceCapabilities.maxImageCount < minImageCount) {
        minImageCount = surfaceCapabilities.maxImageCount;
    }

    // image extent
    // NOTE: has to do this because of sentinel value
    VkExtent2D imageExtent{};
    bool isLimited{surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()};
    if (isLimited) {
        imageExtent = surfaceCapabilities.currentExtent;
    } else {
        auto [width, height] = window.getFramebufferSize();
        imageExtent.width = std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        imageExtent.height = std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    // NOTE: queueFamilyIndexCount, and pQueueFamilyIndices are not set because we are using exclusive sharing mode
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext                 = nullptr,
        .flags                 = {},
        .surface               = m_surface,
        .minImageCount         = minImageCount,
        .imageFormat           = VK_FORMAT_B8G8R8A8_SRGB,  // NOTE: HowToVulkan says these two are guaranteed everywhere
        .imageColorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent           = imageExtent,
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = VK_PRESENT_MODE_FIFO_KHR,  // always supported
        .clipped               = VK_TRUE,
        .oldSwapchain          = VK_NULL_HANDLE
    };

    checkResult(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain), "Error: failed to create Vulkan Swapchain");
    uint32_t imageCount{};
    checkResult(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr), "Error: failed to grab Vulkan Swapchain image count");
    checkResult(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()), "Error: failed to fill Vulkan swapchain image array");
}

void Renderer::createImageViews() {
    VkImageViewCreateInfo imageViewCreateInfo{
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = nullptr,
        .flags      = {},
        .image      = VK_NULL_HANDLE,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = VK_FORMAT_B8G8R8A8_SRGB,
        .components = {},  // no swizzling
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    for (auto [i, image] : std::views::enumerate(m_swapchainImages)) {
        imageViewCreateInfo.image = image;
        checkResult(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_swapchainImageViews[i]), "Error: failed to create Vulkan swapchain image view");
    }
}

void Renderer::checkResult(VkResult result, const char *errorMessage) const {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(errorMessage);
    }
}

void Renderer::cleanup() {
    vkDeviceWaitIdle(m_device);
    swapchainCleanup();
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void Renderer::swapchainCleanup() {
    for (VkImageView imageView : m_swapchainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}
