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
    createImageViews();
    createGraphicsPipeline();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
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

    for (auto [i, queueFamilyProperty] : std::views::enumerate(queueFamilyProperties)) {
        if (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_queueIndex = i;
            break;
        }
    }

    float queuePriority{1.0f};  
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = {},
        .queueFamilyIndex = m_queueIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    // structure chain for our used extensions & core features
    VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{
        .sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR,
        .pNext                    = nullptr,
        .unifiedImageLayouts      = VK_TRUE,
        .unifiedImageLayoutsVideo = VK_FALSE
    };

    // NOTE: shaderDrawParameters was needed for triangle.spv
    VkPhysicalDeviceVulkan11Features vulkan11Features{
        .sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext                = &unifiedImageLayoutsFeatures,
        .shaderDrawParameters = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = &vulkan11Features,
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
    vkGetDeviceQueue(m_device, m_queueIndex, 0, &m_queue);
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
    bool isLimited{surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()};
    if (isLimited) {
        m_swapchainExtent = surfaceCapabilities.currentExtent;
    } else {
        auto [width, height] = window.getFramebufferSize();
        m_swapchainExtent.width  = std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        m_swapchainExtent.height = std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
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
        .imageExtent           = m_swapchainExtent,
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
    m_swapchainImages.resize(imageCount);
    checkResult(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()), "Error: failed to fill Vulkan swapchain image array");
}

void Renderer::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());

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
    std::vector<char> shaderCode{readFile(PROJECT_ROOT_DIR "src/Shaders/triangle.spv")};
    VkShaderModuleCreateInfo shaderModuleCreateInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = {},
        .codeSize = shaderCode.size(),
        .pCode    = reinterpret_cast<const uint32_t *>(shaderCode.data())
    };

    VkShaderModule vertexShaderModule{VK_NULL_HANDLE}, fragmentShaderModule{VK_NULL_HANDLE};

    checkResult(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &vertexShaderModule), "Error: failed to create vertex shader module");
    checkResult(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &fragmentShaderModule), "Error: failed to create fragment shader module");

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
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
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
        .colorWriteMask      = 
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        }
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
        .layout              = m_pipelineLayout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = {},
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = {}
    };

    checkResult(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline), "Error: failed to create graphics pipeline");

    // 12. cleanup
    vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueIndex
    };

    checkResult(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool), "Error: failed to create Vulkan command pool");
}

void Renderer::createCommandBuffers() {
    m_commandBuffers.resize(Config::maxFramesInFlight);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = Config::maxFramesInFlight
    };

    checkResult(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, m_commandBuffers.data()), "Error: failed to allocate Vulkan command buffers");
}

void Renderer::createSyncObjects() {
    m_renderWaitFences.resize(Config::maxFramesInFlight);
    m_acquireImageSemaphores.resize(Config::maxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_swapchainImages.size());

    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {}
    };
    
    for (int i : std::views::iota(0, Config::maxFramesInFlight)) {
        checkResult(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_renderWaitFences[i]), "Error: failed to create render wait fence");
        checkResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_acquireImageSemaphores[i]), "Error: failed to create acquire image semaphore");
    }

    for (int i : std::views::iota(0uz, m_swapchainImages.size())) {
        checkResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]), "Error: failed to create render finished semaphore");
    }
}

void Renderer::drawFrame() {
    // 1. Wait for previous submission to finish rendering before we record commands
    checkResult(vkWaitForFences(m_device, 1, &m_renderWaitFences[m_frameIndex], VK_TRUE, UINT64_MAX), "Error: failed to wait render wait fence");
    checkResult(vkResetFences(m_device, 1, &m_renderWaitFences[m_frameIndex]), "Error: failed to reset render wait fence");

    // 2. Record command buffer
    uint32_t imageIndex{};
    checkResult(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_acquireImageSemaphores[m_frameIndex], VK_NULL_HANDLE, &imageIndex), "Error: failed to acquire swapchain image");
    VkCommandBufferBeginInfo commandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    checkResult(vkBeginCommandBuffer(m_commandBuffers[m_frameIndex], &commandBufferBeginInfo), "Error: failed to begin command buffer");

    // Transition for rendering
    VkImageMemoryBarrier2 imageMemoryBarrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask       = {},
        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_swapchainImages[imageIndex],
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    }; 

    VkDependencyInfo dependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = {},
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &imageMemoryBarrier
    };
    vkCmdPipelineBarrier2(m_commandBuffers[m_frameIndex], &dependencyInfo);

    // Dynamic Rendering
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = m_swapchainImageViews[imageIndex],
        .imageLayout        = VK_IMAGE_LAYOUT_GENERAL,
        .resolveMode        = {},
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = {
            .color        = {0.0f, 0.0f, 0.0f, 1.0f},
        }
    };

    VkRenderingInfo renderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext                = nullptr,
        .flags                = {},
        .renderArea           = {
            .offset = {0, 0},
            .extent = m_swapchainExtent
        },
        .layerCount           = 1,
        .viewMask             = {},
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachmentInfo,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr,
    };

    // Start recording render commands & end once done
    vkCmdBeginRendering(m_commandBuffers[m_frameIndex], &renderingInfo);

    VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(m_swapchainExtent.width),
        .height   = static_cast<float>(m_swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(m_commandBuffers[m_frameIndex], 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {m_swapchainExtent}
    };
    vkCmdSetScissor(m_commandBuffers[m_frameIndex], 0, 1, &scissor);

    vkCmdBindPipeline(m_commandBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    vkCmdDraw(m_commandBuffers[m_frameIndex], 3, 1, 0, 0);

    vkCmdEndRendering(m_commandBuffers[m_frameIndex]);

    // Transition for presentation
    VkImageMemoryBarrier2 presentationTransitionBarrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask       = VK_ACCESS_2_NONE,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_swapchainImages[imageIndex],
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    }; 

    VkDependencyInfo presentationDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = {},
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &presentationTransitionBarrier
    };
    vkCmdPipelineBarrier2(m_commandBuffers[m_frameIndex], &presentationDependencyInfo);

    checkResult(vkEndCommandBuffer(m_commandBuffers[m_frameIndex]), "Error: failed to end command buffer recording");

    // 3. Submit command buffer
    VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{
        .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext       = nullptr,
        .semaphore   = m_acquireImageSemaphores[m_frameIndex],
        .value       = {},
        .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .deviceIndex = 0,
    };

    VkCommandBufferSubmitInfo commandBufferSubmitInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = m_commandBuffers[m_frameIndex],
        .deviceMask    = 0
    };

    VkSemaphoreSubmitInfo renderFinishedSemaphoreSubmitInfo{
        .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext       = nullptr,
        .semaphore   = m_renderFinishedSemaphores[imageIndex],
        .value       = {},
        .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .deviceIndex = 0,
    };

    VkSubmitInfo2 submitInfo{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext                    = nullptr,
        .flags                    = {},
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &waitSemaphoreSubmitInfo,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &commandBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &renderFinishedSemaphoreSubmitInfo
    };
    checkResult(vkQueueSubmit2(m_queue, 1, &submitInfo, m_renderWaitFences[m_frameIndex]), "Error: failed to submit command buffer to queue");

    // 4. Present image
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &m_renderFinishedSemaphores[imageIndex],
        .swapchainCount     = 1,
        .pSwapchains        = &m_swapchain,
        .pImageIndices      = &imageIndex,
        .pResults           = nullptr
    };

    checkResult(vkQueuePresentKHR(m_queue, &presentInfo), "Error: failed to present swapchain image");

    m_frameIndex = (m_frameIndex + 1) % Config::maxFramesInFlight;
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
    syncObjectCleanup();
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
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

void Renderer::syncObjectCleanup() {
    for (int i : std::views::iota(0, Config::maxFramesInFlight)) {
        vkDestroyFence(m_device, m_renderWaitFences[i], nullptr);
        vkDestroySemaphore(m_device, m_acquireImageSemaphores[i], nullptr);
    }

    for (int i : std::views::iota(0uz, m_swapchainImages.size())) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
    }
}
