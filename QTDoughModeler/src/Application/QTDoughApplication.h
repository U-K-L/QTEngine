#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <optional>
#include <cstdint> 
#include <limits> 
#include <algorithm>
//Globals.
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
}
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//QTDough Class.
class QTDoughApplication {
public:
    //Methods.
    int Run();
    void Cleanup();
    //Fields.
    SDL_Window* QTSDLWindow;
    int SCREEN_WIDTH = 640;
    int SCREEN_HEIGHT = 520;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkSwapchainKHR swapChain;


private:
    //Methods.
    void InitSDLWindow();
    void InitVulkan();
    void MainLoop();
    void CreateInstance();
    bool CheckValidationLayerSupport();
    void PickPhysicalDevice();
    bool IsDeviceSuitable(VkPhysicalDevice device);
    void CreateLogicalDevice();
    void CreateWindowSurface();
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    void CreateSwapChain();
    void CreateImageViews();
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    std::vector<const char*> GetRequiredExtensions();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    //Fields.
    VkInstance _vkInstance;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkDevice _logicalDevice = VK_NULL_HANDLE;
    VkQueue _vkGraphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;
    SDL_Surface* _screenSurface = NULL;
    VkQueue _presentQueue = VK_NULL_HANDLE;
    VkDeviceCreateInfo _createInfo{};
    VkSwapchainKHR _swapChain;
};