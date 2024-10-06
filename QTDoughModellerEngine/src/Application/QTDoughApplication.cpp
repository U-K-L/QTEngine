#include "QTDoughApplication.h"
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_system.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <set>

//extern SDL_Window *SDLWindow;
int QTDoughApplication::Run() {
    InitSDLWindow();
	InitVulkan();
	return 0;
}

void QTDoughApplication::InitSDLWindow()
{
    //The surface contained by the window
    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    }
    else
    {
        //Create window
        QTSDLWindow = SDL_CreateWindow("QTDough", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (QTSDLWindow == NULL)
        {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
        else
        {
            //Get window surface
            _screenSurface = SDL_GetWindowSurface(QTSDLWindow);

            //Fill the surface white
            SDL_FillRect(_screenSurface, NULL, SDL_MapRGB(_screenSurface->format, 0xFF, 0xFF, 0xFF));

            //Update the surface
            SDL_UpdateWindowSurface(QTSDLWindow);

            printf("Window Created!!!\n");

            //Hack to get window to stay up
            //SDL_Event e; bool quit = false; while (quit == false) { while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; } }


        }
    }
}

void QTDoughApplication::InitVulkan()
{

	CreateInstance();
    CreateWindowSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();



    CreateSwapChain();
    CreateImageViews();
    CreateGraphicsPipeline();
    
}

void QTDoughApplication::CreateGraphicsPipeline()
{
    printf("Create Graphics Pipeline\n");
    auto vertShaderCode = readFile("src/shaders/vert.spv");
    auto fragShaderCode = readFile("src/shaders/frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    vkDestroyShaderModule(_logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(_logicalDevice, vertShaderModule, nullptr);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
}

VkShaderModule QTDoughApplication::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void QTDoughApplication::CreateImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(_logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void QTDoughApplication::CreateSwapChain() {

    std::cout << "Creating swap chain" << std::endl;
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _vkSurface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(_logicalDevice , _swapChain, &imageCount, nullptr);
    std::cout << "Created swap chain" << std::endl;
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void QTDoughApplication::CreateInstance()
{
    unsigned int extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo createInfo{};
    
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL); //Get the count.
    const char** extensions = (const char**)malloc(sizeof(char*) * extensionCount); //Well justified ;p
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, extensions); //Get the extensions.

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "QTDough";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "QTDoughEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0;

    auto sdlExtensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(sdlExtensions.size());
    createInfo.ppEnabledExtensionNames = sdlExtensions.data();

    if (enableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    //Finally create the instance.
    VkResult result = vkCreateInstance(&createInfo, nullptr, &_vkInstance);
    if (vkCreateInstance(&createInfo, nullptr, &_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

}

bool QTDoughApplication::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void QTDoughApplication::PickPhysicalDevice()
{
    
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, nullptr);

    std::cout << "Device count: " << deviceCount << std::endl;

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            _physicalDevice = device;
            break;
        }
    }

    if (_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU! Ensure you have GPU that supports Ray Tracing Acceleration");
    }

    std::cout << "Suitable device found" << std::endl;
}

bool QTDoughApplication::IsDeviceSuitable(VkPhysicalDevice device) {

    //Set up device features / properties struct.
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    //After getting GPU features create list of features we want GPU to have to run program.

    //Must support raytracing.
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rayTracingFeatures;
    
    //Get the features of this device. Does it support ray tracing?
    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    //return indices.isComplete() && extensionsSupported && swapChainAdequate;

    //return true;

    return rayTracingFeatures.rayTracingPipeline &&
           indices.graphicsFamily.has_value();
}

std::vector<const char*> QTDoughApplication::GetRequiredExtensions() {
    uint32_t extensionCount = 0;

    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL);
    const char** sdlExtensions = (const char**)malloc(sizeof(char*) * extensionCount);
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, sdlExtensions);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}


QueueFamilyIndices QTDoughApplication::FindQueueFamilies(VkPhysicalDevice device) {

    std::optional<uint32_t> graphicsFamily;
    graphicsFamily = 0;

    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    VkBool32 presentSupport = false;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _vkSurface, &presentSupport);
        }

        if (presentSupport) {
            indices.presentFamily = i;
        }

        i++;
    }
    
    return indices;
}

void QTDoughApplication::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    _createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(_physicalDevice, &deviceFeatures);

    _createInfo.pQueueCreateInfos = &queueCreateInfo;
    _createInfo.queueCreateInfoCount = 1;

    _createInfo.pEnabledFeatures = &deviceFeatures;

    _createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    _createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        _createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        _createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        _createInfo.enabledLayerCount = 0;
    }

    //Finally instantiate this device.
    if (vkCreateDevice(_physicalDevice, &_createInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(_logicalDevice, indices.graphicsFamily.value(), 0, &_vkGraphicsQueue);
}

void QTDoughApplication::CreateWindowSurface()
{

    if (!SDL_Vulkan_CreateSurface(QTSDLWindow, _vkInstance, &_vkSurface)) {
        throw std::runtime_error("Failed to create window surface!");
    }
    std::cout << "Created Window Surface" << std::endl;


    /*
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
    */
}


bool QTDoughApplication::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails QTDoughApplication::QuerySwapChainSupport(VkPhysicalDevice device) {
    std::cout << "Swap chain being queried" << std::endl;
    SwapChainSupportDetails details;

    VkResult result;

    // Get surface capabilities
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _vkSurface, &details.capabilities);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface capabilities: " << result << std::endl;
        // Handle the error appropriately (e.g., return, throw exception)
        return details;
    }

    // Get surface formats
    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        std::cerr << "Failed to get physical device surface formats: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.formats.resize(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, details.formats.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface formats data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    // Get present modes
    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        std::cerr << "Failed to get physical device surface present modes: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.presentModes.resize(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, details.presentModes.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface present modes data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    std::cout << "Swap chain support details acquired successfully." << std::endl;
    return details;
}

VkSurfaceFormatKHR QTDoughApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR QTDoughApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D QTDoughApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        SDL_GetWindowSize(QTSDLWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }

}

void QTDoughApplication::Cleanup()
{
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(_logicalDevice, imageView, nullptr);
    }
    vkDestroyInstance(_vkInstance, nullptr);
    vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
    vkDestroyDevice(_logicalDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    SDL_DestroyWindow(QTSDLWindow);
    SDL_Quit();
}