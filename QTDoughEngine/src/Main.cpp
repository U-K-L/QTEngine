#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include <SDL2/SDL.h>
#include <stdio.h>

#include <iostream>
#include <vulkan/vulkan.h>
#include "Application/QTDoughApplication.h"
#include "Engine\Core\InputManager.h"
#include "Loader.h"
#include "UnigmaNative/UnigmaNative.h"
#include <mutex>
#include <thread>

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
int SCREEN_WIDTH = 1280;
int SCREEN_HEIGHT = 720;

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
    int result = std::system("src\\shaders\\Compile.bat");
    if (result != 0) {
        // Optional: handle error
        printf("Shader compilation failed with exit code %d\n", result);
    }
    else {
        printf("Shader compilation succeeded.\n");
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

			if (renderElapsed.count() >= 1)
			{
                GetInput();
				//Render the scene.
                //Check synchronization point for QTDoughApplication.
                //std::cout << "Waiting for signal..." << std::endl;
                {
                    std::unique_lock<std::mutex> lock(QTDoughEngine->shared.mtx);

                    if (QTDoughEngine->shared.requestSignal == false)
                    {
                        //Ask renderer to pause. Wait until it finishes current frame.
                        QTDoughEngine->shared.requestSignal = true;
                        lock.unlock();
                        QTDoughEngine->shared.cvWorker.notify_one();
                        lock.lock();

                        QTDoughEngine->shared.cvMain.wait(lock, [&] {
                            return QTDoughEngine->shared.workFinished;
                        });
                        QTDoughEngine->shared.workFinished = false;
                        lock.unlock();

                        //The worker thread is now waiting. Update its data.
                        UpdateRenderApplication();
                        renderUpdateStart = std::chrono::high_resolution_clock::now();

                        //Notify the worker thread to continue.
                        {
                            std::lock_guard<std::mutex> lg(QTDoughEngine->shared.mtx);
                            QTDoughEngine->shared.continueSignal = true;
                        }
                        QTDoughEngine->shared.cvWorker.notify_one();
                    }
                    else
                    {
                        lock.unlock();
                        std::this_thread::yield();
                    }
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