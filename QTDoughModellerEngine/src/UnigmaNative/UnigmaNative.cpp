#include "UnigmaNative.h"
#include "../Application/AssetLoader.h"
#include "../Application/QTDoughApplication.h"
#include "tiny_gltf.h"
#include "json.hpp"
#include "../Engine/Renderer/UnigmaRenderingObject.h"

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
    uint32_t sizeOfRenderObjs = UNGetRenderObjectsSize();
    //Feed RenderObjects to QTDoughEngine.
    for (uint32_t i = 0; i < sizeOfRenderObjs; i++)
    {
        
        UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(i);
        UnigmaGameObject* gObj = UNGetGameObject(renderObj->GID);
        //We have both the game object and rendering object associated with it, now get the gltf data.
         
        //Node
        const auto node = jsonData["nodes"][gObj->RenderID];

        //Get the ID of this mesh.
        const auto ID = node["name"];

        UnigmaRenderingStructCopyableAttributes renderCopy = UnigmaRenderingStructCopyableAttributes();
        renderCopy._transform = gObj->transform;

        /*
        //Get the relevant information from the buffers.
        const auto& accessors = model.accessors;
        const auto& bufferViews = model.bufferViews;
        const auto& buffers = model.buffers;

        //Our vertex information.
        std::vector<std::array<float, 3>> vertices;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<float, 2>> texcoords;
        std::vector<uint32_t> indices;
        */



        QTDoughApplication::instance->AddRenderObject(&renderCopy, gObj, i);
        std::cout << "Loading Model: " << ID << std::endl;
    }

    std::cout << "Scene Loaded!" << std::endl;

    std::cout << "Model Information and Material Size: " << model.materials.size() << std::endl;
}