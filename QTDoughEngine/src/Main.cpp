#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include <SDL2/SDL.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <vulkan/vulkan.h>
#include "Application/QTDoughApplication.h"
#include "Engine\Core\InputManager.h"
#include "Loader.h"
#include "UnigmaNative/UnigmaNative.h"
#include <mutex>

// Streambuf that writes to both the console and a log file.
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf* consoleBuf, std::streambuf* fileBuf)
        : consoleBuf(consoleBuf), fileBuf(fileBuf) {}
protected:
    int overflow(int c) override {
        if (c == EOF) return !EOF;
        int r1 = consoleBuf->sputc(c);
        int r2 = fileBuf->sputc(c);
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::streamsize r1 = consoleBuf->sputn(s, n);
        std::streamsize r2 = fileBuf->sputn(s, n);
        return (r1 < r2) ? r1 : r2;
    }
    int sync() override {
        int r1 = consoleBuf->pubsync();
        int r2 = fileBuf->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }
private:
    std::streambuf* consoleBuf;
    std::streambuf* fileBuf;
};

static std::ofstream g_logFile;
static TeeBuf* g_coutTee = nullptr;
static TeeBuf* g_cerrTee = nullptr;

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ex) {
    std::cerr << "[CRASH] Unhandled exception 0x" << std::hex << ex->ExceptionRecord->ExceptionCode
              << " at address 0x" << ex->ExceptionRecord->ExceptionAddress << std::endl;
    if (g_logFile.is_open()) {
        g_logFile.flush();
        g_logFile.close();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

UnigmaThread* QTDoughEngine;
QTDoughApplication qtDoughApp;
SDL_Window* QTSDLWindow;
SDL_Surface* _screenSurface = NULL;
int SCREEN_WIDTH = 720;
int SCREEN_HEIGHT = 512;

void RunQTDough()
{
    std::cout << "Houston we're running the renderer!!!" << std::endl;
    qtDoughApp.QTDoughEngineThread = QTDoughEngine;
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
    qtDoughApp.PROGRAMEND = INPUTPROGRAMEND;
    qtDoughApp.framebufferResized = inputFramebufferResized;
}

//This is only called after proper synchronization.
void UpdateRenderApplication()
{
    uint32_t sizeOfGameObjs = UNGetRenderObjectsSize();
    for (uint32_t i = 0; i < sizeOfGameObjs; i++)
    {
        UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(i);
        UnigmaGameObject* gObj = UNGetGameObject(renderObj->GID);
        QTDoughApplication::instance->UpdateObjects(renderObj, gObj, i);
    }

}

int CompileShader()
{
    FILE* pipe = _popen("src\\shaders\\Compile.bat 2>&1", "r");
    if (!pipe) {
        std::cerr << "Failed to run shader compile script" << std::endl;
        return 1;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::cout << buf;
    }
    int result = _pclose(pipe);
    if (result != 0) {
        std::cout << "Shader compilation failed with exit code " << result << std::endl;
    }
    else {
        std::cout << "Shader compilation succeeded." << std::endl;
    }
    return 0;
}

int main(int argc, char* args[]) {

    // Set up logging: tee stdout/stderr to qtdough_log.txt
    g_logFile.open("qtdough_log.txt", std::ios::out | std::ios::trunc);
    if (g_logFile.is_open()) {
        g_coutTee = new TeeBuf(std::cout.rdbuf(), g_logFile.rdbuf());
        g_cerrTee = new TeeBuf(std::cerr.rdbuf(), g_logFile.rdbuf());
        std::cout.rdbuf(g_coutTee);
        std::cerr.rdbuf(g_cerrTee);
    }
    SetUnhandledExceptionFilter(CrashHandler);

    CompileShader();
    QTDoughApplication::SetInstance(&qtDoughApp);

    //Create the window.
    InitSDLWindow();

    qtDoughApp.QTSDLWindow = QTSDLWindow;
    qtDoughApp._screenSurface = _screenSurface;
    qtDoughApp.SCREEN_WIDTH = SCREEN_WIDTH;
    qtDoughApp.SCREEN_HEIGHT = SCREEN_HEIGHT;

    unigmaNative = LoadDLL(L"UnigmaNative.dll");
    LoadUnigmaNativeFunctions();

    UNStartProgram();

    try {
        QTDoughEngine = new UnigmaThread(RunQTDough);

        //initial time.
        auto fixedUpdateStart = std::chrono::high_resolution_clock::now();
        auto renderUpdateStart = std::chrono::high_resolution_clock::now();
        while (qtDoughApp.PROGRAMEND == false)
        {
            //std::cout << "Main Thread Running..." << std::endl;
            //check if 33ms has elasped.
            auto fixedUpdateEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> fixedElapsed = fixedUpdateEnd - fixedUpdateStart;

            if (fixedElapsed.count() >= 33)
			{

                //Update native plugin.
                UNUpdateProgram();

                //Reset timer.
                fixedUpdateStart = std::chrono::high_resolution_clock::now();


			}
            
			//check if 33ms has elapsed.
			auto renderUpdateEnd = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> renderElapsed = renderUpdateEnd - renderUpdateStart;

			if (renderElapsed.count() >= 0)
			{
                GetInput();
				//Render the scene.
                //Check synchronization point for QTDoughApplication.
                //std::cout << "Waiting for signal..." << std::endl;
                if (QTDoughEngine->requestSignal.load(std::memory_order_acquire) == false)
                {
                    //Ask Renderer if scene is done rendering. Wait until its done with current iteration.
                    //std::cout << "1) Asking renderer for request..." << std::endl;

                 


                    //std::cout << "2) Waiting for renderer to finish..." << std::endl;
                    //Wait for the renderer to finish.
                    {
                        std::unique_lock<std::mutex> lock(QTDoughEngine->mainmtx);

                        QTDoughEngine->AskRequest();

                        QTDoughEngine->maincv.wait(lock, [&] {
                            return QTDoughEngine->workFinished.load(std::memory_order_acquire);
                            });
                        QTDoughEngine->workFinished.store(false, std::memory_order_release);
                    }

                    //std::cout << "5) Renderer has finished!" << std::endl;

                    //The worker thread is now waiting for a signal to continue. Update some of its data!
                    UpdateRenderApplication();
                    renderUpdateStart = std::chrono::high_resolution_clock::now();


                    //std::cout << "6) Updated objects, now notify renderer to continue!" << std::endl;
                    //Notify the worker thread to continue.
                    QTDoughEngine->continueSignal.store(true);
                    QTDoughEngine->NotifyWorker();

                }

			}
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    //Clean up and delete threads.
    std::cout << "Cleaning up..." << std::endl;
    //if (QTDoughEngine->thread.joinable())
    //    QTDoughEngine->thread.join();
    UNEndProgram();
    FreeLibrary(unigmaNative);
    unigmaNative = nullptr;
    qtDoughApp.Cleanup();
    delete QTDoughEngine;
;
    return EXIT_SUCCESS;
}