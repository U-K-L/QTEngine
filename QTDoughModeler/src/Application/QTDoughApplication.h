#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <optional>
//Globals.
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
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
    std::vector<const char*> GetRequiredExtensions();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    //Fields.
    VkInstance _vkInstance;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkDevice _logicalDevice = VK_NULL_HANDLE;
    VkQueue _vkGraphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;
    SDL_Surface* _screenSurface = NULL;
    VkQueue _presentQueue = VK_NULL_HANDLE;
    VkDeviceCreateInfo _createInfo{};
};