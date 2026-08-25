#include "Renderer.h"
#include "Config.h"
#include "Window/Window.h"
#include "volk.h"
#include "vk_mem_alloc.h"
#include <ranges>
#include <string>
#include <fstream>
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
    // Grab surface capabilities, minImageCount, imageExtent, surface format
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
        imageExtent.width  = std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        imageExtent.height = std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    // surface format
    // NOTE: HowToVulkan says these two are guaranteed everywhere (if swapchain is supported)
    m_swapchainSurfaceFormat.format     = VK_FORMAT_B8G8R8A8_SRGB;
    m_swapchainSurfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    // NOTE: queueFamilyIndexCount, and pQueueFamilyIndices are not set because we are using exclusive sharing mode
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext                 = nullptr,
        .flags                 = {},
        .surface               = m_surface,
        .minImageCount         = minImageCount,
        .imageFormat           = m_swapchainSurfaceFormat.format,
        .imageColorSpace       = m_swapchainSurfaceFormat.colorSpace,
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
        .format     = m_swapchainSurfaceFormat.format,
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

void Renderer::createGraphicsPipeline() {
    // 1. Shader stage
    // create shader modules first
    std::vector<char> shaderCode{readFile(PROJECT_ROOT_DIR "src/shaders/triangle.spv")};
    VkShaderModuleCreateInfo shaderModuleCreateInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = {},
        .codeSize = shaderCode.size(),
        .pCode    = reinterpret_cast<const uint32_t *>(shaderCode.data())
    };

    VkShaderModule vertexShaderModule{VK_NULL_HANDLE}, fragmentShaderModule{VK_NULL_HANDLE};

    vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &vertexShaderModule);
    vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &fragmentShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShaderModule,
        .pName  = "vertexMain",
        .pSpecializationInfo = nullptr},

        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentShaderModule,
        .pName = "fragmentMain",
        .pSpecializationInfo = nullptr}
    };

    // 2. Vertex Input State & Input Assembly State
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = {},
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = {},
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // 3. Viewport State & Scissor State
    // NOTE: these are dynamic
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .viewportCount = 0,
        .pViewports = nullptr,
        .scissorCount = 0,
        .pScissors = nullptr
    };

    // 4. Rasterization State
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = {},
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f
    };

    // 5. Multisample State
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = {},
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 0.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE
    };

    // 6. Depth Stencil State
    // TODO: will need to enable depth testing later.
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = {},
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {},
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f
    };

    // 7. Color Blend State
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates{
        {.blendEnable        = VK_FALSE,
        .srcColorBlendFactor = {},
        .dstColorBlendFactor = {},
        .colorBlendOp        = {},
        .srcAlphaBlendFactor = {},
        .dstAlphaBlendFactor = {},
        .alphaBlendOp        = {},
        .colorWriteMask = {
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        }}
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = {},
        .logicOpEnable   = VK_FALSE,
        .logicOp         = {},
        .attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size()),
        .pAttachments    = colorBlendAttachmentStates.data(),
        .blendConstants  = {}
    };

    // 8. Dynamic State
    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = {},
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data()
    };

    // 9. Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = {},
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr
    };
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

    // 10. Dynamic Rendering
    VkPipelineRenderingCreateInfo renderingCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .viewMask                = {},
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &m_swapchainSurfaceFormat.format,
        .depthAttachmentFormat   = {},
        .stencilAttachmentFormat = {}
    };

    // 11. Finally create graphics pipeline
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderingCreateInfo,
        .flags               = {},
        .stageCount          = static_cast<uint32_t>(shaderStages.size()),
        .pStages             = shaderStages.data(),
        .pVertexInputState   = &vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssemblyStateCreateInfo,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizationStateCreateInfo,
        .pMultisampleState   = &multisampleStateCreateInfo,
        .pDepthStencilState  = &depthStencilStateCreateInfo,
        .pColorBlendState    = &colorBlendStateCreateInfo,
        .pDynamicState       = &dynamicStateCreateInfo,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = {},
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = {}
    };

    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline);
}

std::vector<char> Renderer::readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Error: failed to open file");
    }

    std::vector<char> fileContents(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(fileContents.data(), static_cast<std::streamsize>(fileContents.size()));
    file.close();
    return fileContents;
}

void Renderer::checkResult(VkResult result, const char *errorMessage) const {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(errorMessage);
    }
}

void Renderer::cleanup() {
    vkDeviceWaitIdle(m_device);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
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
