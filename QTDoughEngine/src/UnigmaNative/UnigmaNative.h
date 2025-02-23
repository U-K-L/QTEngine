#pragma once
#include <windows.h>
#include <mutex> 
#include <thread>
#include <chrono>
#include "../Engine/Core/UnigmaGameObject.h"
#include "../Engine/Renderer/UnigmaRenderingStruct.h"
#include "../Engine/Camera/UnigmaCamera.h"
#include "../Engine/Renderer/UnigmaLights.h"



// Define a callback function signature
typedef void (*AppFunctionType)(const char*);
typedef void (*LoadSceneCallbackType)(const char* sceneName);

// Function pointer types for the functions exported from the DLL
typedef int (*FnUnigmaNative)();
typedef void (*FnStartProgram)();
typedef void (*FnEndProgram)();
typedef void (*FnUpdateProgram)();

typedef UnigmaGameObject* (*FnGetGameObject)(uint32_t ID);

typedef UnigmaRenderingStruct* (*FnGetRenderObjectAt)(uint32_t ID);
typedef uint32_t (*FnGetRenderObjectsSize)();

typedef UnigmaCameraStruct* (*FnGetCamera)(uint32_t ID);
typedef uint32_t (*FnGetCamerasSize)();

typedef UnigmaLight* (*FnGetLight)(uint32_t ID);
typedef uint32_t(*FnGetLightsSize)();

typedef void (*FnRegisterCallback)(AppFunctionType); //Generic function pointer type for registering a callback function
typedef void (*FnRegisterLoadSceneCallback)(LoadSceneCallbackType); //Generic function pointer type for registering a callback function


// Function pointers for the functions exported from the DLL
extern FnStartProgram UNStartProgram;
extern FnEndProgram UNEndProgram;
extern FnUpdateProgram UNUpdateProgram;
extern FnGetGameObject UNGetGameObject;
extern FnGetRenderObjectAt UNGetRenderObjectAt;
extern FnGetRenderObjectsSize UNGetRenderObjectsSize;
extern FnGetCamera UNGetCamera;
extern FnGetCamerasSize UNGetCamerasSize;
extern FnGetLight UNGetLight;
extern FnGetLightsSize UNGetLightsSize;
extern FnRegisterCallback UNRegisterCallback;
extern FnRegisterLoadSceneCallback UNRegisterLoadSceneCallback;


void ApplicationFunction(const char* message);
void LoadScene(const char* sceneName);

extern HMODULE unigmaNative;

using namespace std;

void LoadUnigmaNativeFunctions();

class UnigmaThread
{
public:
	std::thread thread;
	condition_variable cv;
	mutex mtx;
	atomic<bool> isSleeping();
    std::atomic<bool> shouldTerminate{ false }; // Flag to signal thread termination
	UnigmaThread(void (*func)())
	{
		thread = std::thread(func);
	}

    UnigmaThread(std::function<void()> func) {
        thread = std::thread([this, func]() {
            while (!shouldTerminate) {
                func(); // Run the provided function
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Example sleep to avoid tight loop
            }
            });
    }

    ~UnigmaThread() {
        shouldTerminate = true; // Signal the thread to terminate
        if (thread.joinable()) {
            thread.join(); // Ensure the thread finishes before destruction
        }
    }
};

