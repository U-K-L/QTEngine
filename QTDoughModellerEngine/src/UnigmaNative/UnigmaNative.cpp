#include "UnigmaNative.h"


// Define the global variables here
FnStartProgram UNStartProgram;
FnEndProgram UNEndProgram;
FnUpdateProgram UNUpdateProgram;
FnGetGameObject UNGetGameObject;
FnGetRenderObjectAt UNGetRenderObjectAt;
FnGetRenderObjectsSize UNGetRenderObjectsSize;
FnRegisterCallback UNRegisterCallback;
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

    //Register the callback function

}

void ApplicationFunction(const char* message) {
    std::cout << "Application received message: " << message << std::endl;
}