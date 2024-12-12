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

    nlohmann::json glfJson;
    inputFile >> glfJson;
    inputFile.close();

    //open normal json file.
    std::ifstream inputFile2(AssetsPath + "Scenes/" + sceneName + ".json");
    if (!inputFile2.is_open()) {
		std::cerr << "Error: Unable to open file!" << std::endl;
	}

    nlohmann::json sceneJson;
    inputFile2 >> sceneJson;
    inputFile2.close();

    //Now loop through game objects getting the associated node for each one.
    uint32_t sizeOfRenderObjs = UNGetRenderObjectsSize();


    std::cout << "Size of Render Objects: " << sizeOfRenderObjs << std::endl;

    //Feed RenderObjects to QTDoughEngine.
    for (uint32_t i = 0; i < sizeOfRenderObjs; i++)
    {
        
        UnigmaRenderingStruct* renderObj = UNGetRenderObjectAt(i);
        UnigmaGameObject* gObj = UNGetGameObject(renderObj->GID);
        const auto GameObjectJson = sceneJson["GameObjects"][gObj->ID];

        std::cout << "GameObject ID: " << gObj->ID << std::endl;

        // Ensure both objects are valid
        if (!renderObj || !gObj) {
            std::cerr << "Invalid render object or game object at index: " << i << std::endl;
            continue;
        }
        //Node
        const auto node = glfJson["nodes"][gObj->RenderID];

        //Get the name of this mesh.
        const auto name = node["name"];

        //print name.
        std::cout << "Name: " << name << std::endl;
        int meshID = 0;
        // Check if the node contains a mesh
        if (node.contains("mesh")) {
            // Get the mesh ID
            meshID = node["mesh"];
        }
        else {
            continue;
        }

        UnigmaRenderingStructCopyableAttributes renderCopy = UnigmaRenderingStructCopyableAttributes();
        renderCopy._transform = gObj->transform;

        // Access CustomProperties
        if (GameObjectJson.contains("CustomProperties")) {

            //Assign material properties
            auto customProperties = sceneJson["GameObjects"][gObj->ID]["customProperties"];

            //Loop through all custom properties as a hashmap.
            for (const auto& [propName, propValue] : GameObjectJson["CustomProperties"].items()) {
                if (propValue.is_object()) {
                    // Handle nested properties (e.g., BaseAlbedo)
                    renderCopy._material.vectorProperties[propName] = glm::vec4(propValue["r"], propValue["g"], propValue["b"], propValue["a"]);

                }
                else {
                    // Handle single properties (e.g., TextureName)
					renderCopy._material.vectorProperties[propName] = glm::vec4(propValue, 0.0f, 0.0f, 0.0f);
				
                }
            }
        }

        //Get the relevant information from the buffers.
        const auto& accessors = model.accessors;
        const auto& bufferViews = model.bufferViews;
        const auto& buffers = model.buffers;

        //Our vertex information.
        std::vector<std::array<float, 3>> positions;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<float, 2>> texcoords;
        std::vector<uint32_t> indices;

        //Get vertices positions
        const auto& positionAccessor = accessors[model.meshes[meshID].primitives[0].attributes["POSITION"]];
        const auto& positionBufferView = bufferViews[positionAccessor.bufferView];
        const auto& positionBuffer = buffers[positionBufferView.buffer];
        const auto positionBufferStart = positionBuffer.data.data() + positionBufferView.byteOffset + positionAccessor.byteOffset;
        const auto positionBufferEnd = positionBufferStart + positionAccessor.count * 3 * sizeof(float);
        positions.resize(positionAccessor.count);
        std::memcpy(positions.data(), positionBufferStart, positionAccessor.count * 3 * sizeof(float));

        //Get normals
        const auto& normalAccessor = accessors[model.meshes[meshID].primitives[0].attributes["NORMAL"]];
        const auto& normalBufferView = bufferViews[normalAccessor.bufferView];
        const auto& normalBuffer = buffers[normalBufferView.buffer];
        const auto normalBufferStart = normalBuffer.data.data() + normalBufferView.byteOffset + normalAccessor.byteOffset;
        const auto normalBufferEnd = normalBufferStart + normalAccessor.count * 3 * sizeof(float);
        normals.resize(normalAccessor.count);
        std::memcpy(normals.data(), normalBufferStart, normalAccessor.count * 3 * sizeof(float));

        //Get texcoords
        const auto& texcoordAccessor = accessors[model.meshes[meshID].primitives[0].attributes["TEXCOORD_0"]];
        const auto& texcoordBufferView = bufferViews[texcoordAccessor.bufferView];
        const auto& texcoordBuffer = buffers[texcoordBufferView.buffer];
        const auto texcoordBufferStart = texcoordBuffer.data.data() + texcoordBufferView.byteOffset + texcoordAccessor.byteOffset;
        const auto texcoordBufferEnd = texcoordBufferStart + texcoordAccessor.count * 2 * sizeof(float);
        texcoords.resize(texcoordAccessor.count);
        std::memcpy(texcoords.data(), texcoordBufferStart, texcoordAccessor.count * 2 * sizeof(float));


        std::vector<Vertex> vertices;
        for (size_t i = 0; i < positions.size(); i++)
		{
            Vertex vertex = { .pos = { positions[i][0], positions[i][1], positions[i][2] } };
			vertices.push_back(vertex);
		}

        for(size_t i = 0; i < normals.size(); i++)
		{
			vertices[i].normal = { normals[i][0], normals[i][1], normals[i][2] };
		}

        for(size_t i = 0; i < texcoords.size(); i++)
        {
            vertices[i].texCoord = { texcoords[i][0], texcoords[i][1] };
		}

        //Get indices
        const auto& indexAccessor = accessors[model.meshes[meshID].primitives[0].indices];
        const auto& indexBufferView = bufferViews[indexAccessor.bufferView];
        const auto& indexBuffer = buffers[indexBufferView.buffer];
        const auto indexBufferStart = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        const auto indexBufferEnd = indexBufferStart + indexAccessor.count * sizeof(uint16_t);

        // Copy and convert uint16_t -> uint32_t
        const auto* indexBufferData = reinterpret_cast<const uint16_t*>(indexBufferStart);
        indices.resize(indexAccessor.count);
        for (size_t i = 0; i < indexAccessor.count; i++) {
            indices[i] = static_cast<uint32_t>(indexBufferData[i]);
        }


        UnigmaRenderingStruct renderStruct = UnigmaRenderingStruct();
        renderStruct.vertices = vertices;
        renderStruct.indices = indices;

        renderCopy._renderer = renderStruct;


        QTDoughApplication::instance->AddRenderObject(&renderCopy, gObj, i);
    }

    std::cout << "Scene Loaded!" << std::endl;

    std::cout << "Model Information and Material Size: " << model.materials.size() << std::endl;
}