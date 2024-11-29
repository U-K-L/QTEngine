#include "UnigmaNative.h"
#include "../Application/AssetLoader.h"
#include "../Application/QTDoughApplication.h"
#include "tiny_gltf.h"
#include "json.hpp"
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

//Loads the scene and all its initial models. This may or may not include the player.
void LoadScene(const char* sceneName) {

    //First get the model from tinygltf. We need it to load all the arrays associated with the scene.
    tinygltf::Model model;
    assetLoader.LoadGLTF(AssetsPath + "Scenes/" + sceneName + ".gltf", model);

    //We now have a binary loaded as the Model. And also a JSON describing the hiearchy of the scene. Load the JSON.
    std::ifstream inputFile(AssetsPath + "Scenes/" + sceneName + ".gltf");
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file!" << std::endl;
    }

    nlohmann::json jsonData;
    inputFile >> jsonData;
    inputFile.close();

    //Now loop through game objects getting the associated node for each one.
    uint32_t sizeOfGameObjs = UNGetRenderObjectsSize();
    //Feed RenderObjects to QTDoughEngine.
    for (uint32_t i = 0; i < sizeOfGameObjs; i++)
    {
        UnigmaGameObject* gObj = UNGetGameObject(i);


        //We have both the game object and rendering object associated with it, now get the gltf data.
         
        //Node
        const auto node = jsonData["nodes"][gObj->ID];

        //Get the ID of this mesh.
        const auto ID = node["name"];

        std::cout << gObj->name << std::endl;

        UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(gObj->RenderID);
        QTDoughApplication::instance->AddRenderObject(renderObj, gObj, i);

    }

    std::cout << "Scene Loaded!" << std::endl;

    std::cout << "Model Information and Material Size: " << model.materials.size() << std::endl;
}