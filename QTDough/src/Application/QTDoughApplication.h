#pragma once
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
//Globals.
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

//QTDough Class.
class QTDoughApplication {
public:
    int Run();
    SDL_Window* QTSDLWindow;


private:
    //Methods.
    void InitVulkan();
    void MainLoop();
    void Cleanup();
    void CreateInstance();
    bool checkValidationLayerSupport();
    //Fields.
    VkInstance _vkInstance;
};