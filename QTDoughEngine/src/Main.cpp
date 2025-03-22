#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include <SDL2/SDL.h>
#include <stdio.h>

#include <iostream>
#include <vulkan/vulkan.h>
#include "Application/QTDoughApplication.h"
#include "Loader.h"
#include "UnigmaNative/UnigmaNative.h"
#include <mutex>

UnigmaThread* QTDoughEngine;
QTDoughApplication qtDoughApp;
SDL_Window* QTSDLWindow;
SDL_Surface* _screenSurface = NULL;
int SCREEN_WIDTH = 640;
int SCREEN_HEIGHT = 520;

void RunQTDough()
{
    std::cout << "Houston we're running the renderer!!!" << std::endl;
    qtDoughApp.Run();
}

void InitSDLWindow()
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
            SDL_FillRect(_screenSurface, NULL, SDL_MapRGB(_screenSurface->format, 0x00, 0x00, 0x00));

            //Update the surface
            SDL_UpdateWindowSurface(QTSDLWindow);

            printf("Window Created!!!\n");

            //Hack to get window to stay up
            //SDL_Event e; bool quit = false; while (quit == false) { while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; } }


        }
    }
}

void GetInput()
{

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            std::cout << "Quitting program." << std::endl;
            qtDoughApp.PROGRAMEND = true;
        }
        else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
                qtDoughApp.framebufferResized = true;
            }
        }
        //ImGui_ImplSDL2_ProcessEvent(&e);
    }
}

int main(int argc, char* args[]) {

    QTDoughApplication::SetInstance(&qtDoughApp);

    //Create the window.
    InitSDLWindow();

    qtDoughApp.QTSDLWindow = QTSDLWindow;
    qtDoughApp._screenSurface = _screenSurface;
    qtDoughApp.SCREEN_WIDTH = SCREEN_WIDTH;
    qtDoughApp.SCREEN_HEIGHT = SCREEN_HEIGHT;

    unigmaNative = LoadDLL(L"Unigma/UnigmaNative.dll");
    LoadUnigmaNativeFunctions();

    UNStartProgram();

    try {
        QTDoughEngine = new UnigmaThread(RunQTDough);

        //initial time.
        auto fixedUpdateStart = std::chrono::high_resolution_clock::now();
        auto renderUpdateStart = std::chrono::high_resolution_clock::now();
        while (qtDoughApp.PROGRAMEND == false)
        {
            //check if 33ms has elasped.
            auto fixedUpdateEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> fixedElapsed = fixedUpdateEnd - fixedUpdateStart;

            if (fixedElapsed.count() >= 0.0001)
			{

                //Update native plugin.
                UNUpdateProgram();

                //Reset timer.
                fixedUpdateStart = std::chrono::high_resolution_clock::now();


			}
            
			//check if 3ms has elapsed.
			auto renderUpdateEnd = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> renderElapsed = renderUpdateEnd - renderUpdateStart;

			if (renderElapsed.count() >= 4)
			{
                GetInput();
				//Render the scene.
                //Check synchronization point for QTDoughApplication.
                uint32_t sizeOfGameObjs = UNGetRenderObjectsSize();
                for (uint32_t i = 0; i < sizeOfGameObjs; i++)
                {
                    UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(i);
                    UnigmaGameObject* gObj = UNGetGameObject(renderObj->GID);
                    QTDoughApplication::instance->UpdateObjects(renderObj, gObj, i);
                }

                renderUpdateStart = std::chrono::high_resolution_clock::now();
			}
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    //Clean up and delete threads.
    qtDoughApp.Cleanup();
    delete QTDoughEngine;
    return EXIT_SUCCESS;
}