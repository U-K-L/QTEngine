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

    UnigmaGameObject* gObj = UNGetGameObject(0);



    qtDoughApp.SetInstance(&qtDoughApp);
    try {
        QTDoughEngine = new UnigmaThread(RunQTDough);
        while (true)
        {

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