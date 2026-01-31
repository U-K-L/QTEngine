#pragma once
#include "UnigmaThread.h"
#include "../Engine/Core/UnigmaGameObject.h"
#include "../Engine/Renderer/UnigmaRenderingStruct.h"
#include "../Engine/Camera/UnigmaCamera.h"
#include "../Engine/Renderer/UnigmaLights.h"
#include "../Engine/Core/InputManager.h"



// Define a callback function signature
typedef void (*AppFunctionType)(const char*);
typedef void (*LoadSceneCallbackType)(const char* sceneName);
typedef UnigmaInputStruct (*LoadInputCallbackType)(int flag);

// Function pointer types for the functions exported from the DLL
typedef int (*FnUnigmaNative)();
typedef void (*FnStartProgram)();
typedef void (*FnEndProgram)();
typedef void (*FnUpdateProgram)();


typedef UnigmaRenderingStruct* (*FnGetRenderObjectAt)(uint32_t ID);
typedef uint32_t (*FnGetRenderObjectsSize)();

typedef UnigmaCameraStruct* (*FnGetCamera)(uint32_t ID);
typedef uint32_t (*FnGetCamerasSize)();

typedef UnigmaLight* (*FnGetLight)(uint32_t ID);
typedef uint32_t(*FnGetLightsSize)();

typedef void (*FnRegisterCallback)(AppFunctionType); //Generic function pointer type for registering a callback function
typedef void (*FnRegisterLoadSceneCallback)(LoadSceneCallbackType); //Generic function pointer type for registering a callback function
typedef void (*FnRegisterLoadInputCallback)(LoadInputCallbackType); //Generic function pointer type for registering a callback function






// Function pointers for the functions exported from the DLL
extern FnStartProgram UNStartProgram;
extern FnEndProgram UNEndProgram;
extern FnUpdateProgram UNUpdateProgram;
extern FnGetRenderObjectAt UNGetRenderObjectAt;
extern FnGetRenderObjectsSize UNGetRenderObjectsSize;
extern FnGetCamera UNGetCamera;
extern FnGetCamerasSize UNGetCamerasSize;
extern FnGetLight UNGetLight;
extern FnGetLightsSize UNGetLightsSize;
extern FnRegisterCallback UNRegisterCallback;
extern FnRegisterLoadSceneCallback UNRegisterLoadSceneCallback;
extern FnRegisterLoadInputCallback UNRegisterLoadInputCallback;


void ApplicationFunction(const char* message);
void LoadScene(const char* sceneName);
UnigmaInputStruct LoadInput(int flag);

extern HMODULE unigmaNative;

void LoadUnigmaNativeFunctions();
void LoadGameObjectHooks();

