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

void RunQTDough()
{
    std::cout << "Houston we're running the renderer!!!" << std::endl;
    qtDoughApp.Run();
}

int main(int argc, char* args[]) {

    QTDoughApplication::SetInstance(&qtDoughApp);

    unigmaNative = LoadDLL(L"Unigma/UnigmaNative.dll");
    LoadUnigmaNativeFunctions();

    UNStartProgram();

    try {
        QTDoughEngine = new UnigmaThread(RunQTDough);
        //initial time.
        auto start = std::chrono::high_resolution_clock::now();
        while (true)
        {
            //check if 33ms has elasped.
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            if (elapsed.count() >= 33)
			{

                //Update native plugin.
                UNUpdateProgram();

                //Reset timer.
                start = std::chrono::high_resolution_clock::now();
			}

            //Check synchronization point for QTDoughApplication.
            uint32_t sizeOfGameObjs = UNGetRenderObjectsSize();
            for (uint32_t i = 0; i < sizeOfGameObjs; i++)
            {
                UnigmaGameObject* gObj = UNGetGameObject(i);
                UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(gObj->RenderID);
                QTDoughApplication::instance->UpdateObjects(renderObj, gObj, i);
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