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


    unigmaNative = LoadDLL(L"Unigma/UnigmaNative.dll");
    LoadUnigmaNativeFunctions();

    UNStartProgram();

    uint32_t sizeOFObjts = UNGetRenderObjectsSize();

    std::cout << "\nSize of objects: " << sizeOFObjts << std::endl;

    qtDoughApp.SetInstance(&qtDoughApp);
    //Feed RenderObjects to QTDoughEngine.
    for (uint32_t i = 0; i < sizeOFObjts; i++)
    {
        UnigmaGameObject* gObj = UNGetGameObject(i);
        UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(gObj->RenderID);
        qtDoughApp.AddRenderObject(renderObj, gObj, i);
    }
    try {
        QTDoughEngine = new UnigmaThread(RunQTDough);
        while (true)
        {
            for (uint32_t i = 0; i < sizeOFObjts; i++)
            {
                UnigmaGameObject* gObj = UNGetGameObject(i);
                UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(gObj->RenderID);
                qtDoughApp.UpdateObjects(renderObj, gObj, i);
            }
            //Update native plugin.
            UNUpdateProgram();
            //Wait 33ms before updating again.
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
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