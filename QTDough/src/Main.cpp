#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include <SDL.h>
#include <stdio.h>

#include <iostream>
#include <vulkan/vulkan.h>
#include "Application/QTDoughApplication.h"

int main(int argc, char* args[]) {
    //The window we'll be rendering to
    QTDoughApplication qtDoughApp;

    try {
        qtDoughApp.Run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
    /*
    //Destroy window
    SDL_DestroyWindow(window);

    //Quit SDL subsystems
    SDL_Quit();
    */
    SDL_Quit();
    return 0;
}