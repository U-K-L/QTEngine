#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
//Globals.
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
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
    std::vector<const char*> GetRequiredExtensions();
    //Fields.
    VkInstance _vkInstance;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
};