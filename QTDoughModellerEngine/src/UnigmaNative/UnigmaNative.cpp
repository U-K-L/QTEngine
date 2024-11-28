#include "UnigmaNative.h"
#include "../Application/AssetLoader.h"

AssetLoader assetLoader;

// Define the global variables here
FnStartProgram UNStartProgram;
FnEndProgram UNEndProgram;
FnUpdateProgram UNUpdateProgram;
FnGetGameObject UNGetGameObject;
FnGetRenderObjectAt UNGetRenderObjectAt;
FnGetRenderObjectsSize UNGetRenderObjectsSize;
FnRegisterCallback UNRegisterCallback;
FnRegisterLoadSceneCallback UNRegisterLoadSceneCallback;
HMODULE unigmaNative;


void LoadUnigmaNativeFunctions()
{
    UNStartProgram = (FnStartProgram)GetProcAddress(unigmaNative, "StartProgram");
    UNEndProgram = (FnEndProgram)GetProcAddress(unigmaNative, "EndProgram");
    UNUpdateProgram = (FnUpdateProgram)GetProcAddress(unigmaNative, "UpdateProgram");
    UNGetGameObject = (FnGetGameObject)GetProcAddress(unigmaNative, "GetGameObject");
    UNGetRenderObjectAt = (FnGetRenderObjectAt)GetProcAddress(unigmaNative, "GetRenderObjectAt");
    UNGetRenderObjectsSize = (FnGetRenderObjectsSize)GetProcAddress(unigmaNative, "GetRenderObjectsSize");
    UNRegisterCallback = (FnRegisterCallback)GetProcAddress(unigmaNative, "RegisterCallback");
    UNRegisterLoadSceneCallback = (FnRegisterLoadSceneCallback)GetProcAddress(unigmaNative, "RegisterLoadSceneCallback");

    //Register the callback function
    UNRegisterCallback(ApplicationFunction);
    UNRegisterLoadSceneCallback(LoadScene);
}

void ApplicationFunction(const char* message) {
    std::cout << "Application received message: " << message << std::endl;
}

void LoadScene(const char* sceneName) {
	std::cout << "Renderer is loading the following Scene: " << sceneName << std::endl;
    assetLoader.LoadGLB(sceneName);
}