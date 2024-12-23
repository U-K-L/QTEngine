#include "UnigmaNative.h"
#include "../Application/AssetLoader.h"
#include "../Application/QTDoughApplication.h"
#include "tiny_gltf.h"
#include "json.hpp"
#include "../Engine/Renderer/UnigmaRenderingObject.h"
#include "stb_image.h"

AssetLoader assetLoader;

// Define the global variables here
FnStartProgram UNStartProgram;
FnEndProgram UNEndProgram;
FnUpdateProgram UNUpdateProgram;
FnGetGameObject UNGetGameObject;
FnGetRenderObjectAt UNGetRenderObjectAt;
FnGetRenderObjectsSize UNGetRenderObjectsSize;
FnGetCamera UNGetCamera;
FnGetCamerasSize UNGetCamerasSize;
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
    UNGetCamera = (FnGetCamera)GetProcAddress(unigmaNative, "GetCamera");
    UNGetCamerasSize = (FnGetCamerasSize)GetProcAddress(unigmaNative, "GetCamerasSize");
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

    std::string scenePath = AssetsPath + "Scenes/" + sceneName + "/" + sceneName;
    //First get the model from tinygltf. We need it to load all the arrays associated with the scene.
    tinygltf::Model model;
    assetLoader.LoadGLTF(scenePath + ".gltf", model);

    //We now have a binary loaded as the Model. And also a JSON describing the hiearchy of the scene. Load the JSON.
    std::ifstream inputFile(scenePath + ".gltf");
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file!" << std::endl;
    }

    nlohmann::json glfJson;
    inputFile >> glfJson;
    inputFile.close();

    //open normal json file.
    std::ifstream inputFile2(scenePath + ".json");
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
        const auto GameObjectJson = sceneJson["GameObjects"][gObj->JID];

        //check if the game object has a mesh.
        if(!GameObjectJson.contains("MeshID"))
		{
			std::cerr << "Game Object does not have a mesh!" << std::endl;
			continue;
		}
        else
            {
            std::cout << "Game Object has a mesh!" << std::endl;
			}

        uint32_t gameMeshID = GameObjectJson["MeshID"];

        // Ensure both objects are valid
        if (!renderObj || !gObj) {
            std::cerr << "Invalid render object or game object at index: " << i << std::endl;
            continue;
        }
        //Node
        const auto node = glfJson["nodes"][gameMeshID];


        //Get the name of this mesh.
        const auto name = node["name"];

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

        //Get base albedo.


        //Get the relevant information from the buffers.
        const auto& accessors = model.accessors;
        const auto& bufferViews = model.bufferViews;
        const auto& buffers = model.buffers;

        //Our vertex information.
        std::vector<std::array<float, 3>> positions;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<float, 2>> texcoords;
        std::vector<std::array<float, 3>> colors;
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

        //Get colors
        // Check if the mesh has COLOR_0
        auto& prim = model.meshes[meshID].primitives[0];
        if (prim.attributes.find("COLOR_0") != prim.attributes.end())
        {
            const auto& colorAccessor = accessors[prim.attributes["COLOR_0"]];
            const auto& colorBufferView = bufferViews[colorAccessor.bufferView];
            const auto& colorBuffer = buffers[colorBufferView.buffer];

            // Calculate start pointer of color data
            const auto colorBufferStart = colorBuffer.data.data()
                + colorBufferView.byteOffset
                + colorAccessor.byteOffset;
            // Each color is assumed to be 4 floats
            colors.resize(colorAccessor.count);
            std::memcpy(colors.data(), colorBufferStart, colorAccessor.count * 4 * sizeof(float));
        }
        else
        {
            // If no vertex colors, just give them all a default color (e.g., white)
            colors.resize(positions.size(), { 1.0f, 1.0f, 1.0f });
        }

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

        for (size_t i = 0; i < vertices.size(); i++)
        {
            vertices[i].color = {
                colors[i][0],
                colors[i][1],
                colors[i][2]
            };
        }

        // For each mesh's primitive, look up its assigned material
        int materialIndex = model.meshes[meshID].primitives[0].material;
        if (materialIndex >= 0 && materialIndex < model.materials.size())
        {
            const tinygltf::Material& material = model.materials[materialIndex];

            //----- Example: PBR Metallic Roughness baseColorTexture -----
            int baseColorTexIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorTexIndex >= 0 && baseColorTexIndex < (int)model.textures.size())
            {
                const tinygltf::Texture& baseColorTex = model.textures[baseColorTexIndex];
                // Load only if baseColorTex.source is valid
                if (baseColorTex.source >= 0 && baseColorTex.source < (int)model.images.size())
                {
                    const tinygltf::Image& image = model.images[baseColorTex.source];

                    // Construct texture path (assuming images are in the "Textures" folder)
                    std::string texturePath = AssetsPath + "Textures/" + image.uri;

                    //set texture ID
                    //renderCopy._material.textureIDs[i] = i;
                    std::vector<std::string> tokens;
                    std::istringstream tokenStream(texturePath);
                    std::string token;
                    while (std::getline(tokenStream, token, '/')) {
                        tokens.push_back(token);
                    }

                    // Safely remove .png if present
                    if (!tokens.empty()) {
                        std::string& lastToken = tokens.back();
                        if (lastToken.size() >= 4 && lastToken.compare(lastToken.size() - 4, 4, ".png") == 0) {
                            lastToken.erase(lastToken.end() - 4, lastToken.end());
                        }
                    }

                    std::string textureName = tokens.empty() ? texturePath : tokens.back();
                    renderCopy._material.textures.push_back(UnigmaTexture(texturePath));

                    //assign textureName
                    renderCopy._material.textureNames[0] = textureName;
                }
            }

            /*
            //----- Example: Normal texture -----
            int normalTexIndex = material.normalTexture.index;
            if (normalTexIndex >= 0 && normalTexIndex < (int)model.textures.size())
            {
                const tinygltf::Texture& normalTex = model.textures[normalTexIndex];
                if (normalTex.source >= 0 && normalTex.source < (int)model.images.size())
                {
                    const tinygltf::Image& image = model.images[normalTex.source];
                    std::string texturePath = AssetsPath + "Textures/" + image.uri;
                    std::cout << "Loading normalTexture: " << texturePath << std::endl;

                    // ... load texture the same way ...

                    renderCopy._material.textures.push_back(UnigmaTexture(texturePath));
                }
            }
            */

            //----- Repeat for other textures your material might include, e.g.:
            //      occlusionTexture, emissiveTexture, metallicRoughnessTexture, etc.
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

        /*
        // 2) Compute the bounding box
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();

        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();

        for (int i = 0; i < vertices.size(); i++)
        {
            float x = vertices[i].pos.x;
            float y = vertices[i].pos.y;
            float z = vertices[i].pos.z;

            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (z < minZ) minZ = z;

            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
            if (z > maxZ) maxZ = z;
        }

        // 3) Find the center of the bounding box and the largest half-extent
        float centerX = 0.5f * (minX + maxX);
        float centerY = 0.5f * (minY + maxY);
        float centerZ = 0.5f * (minZ + maxZ);

        float halfExtentX = 0.5f * (maxX - minX);
        float halfExtentY = 0.5f * (maxY - minY);
        float halfExtentZ = 0.5f * (maxZ - minZ);

        float largestHalfExtent = std::max({ halfExtentX, halfExtentY, halfExtentZ });

        // 4) Shift and scale all vertices
        if (largestHalfExtent > 1e-6f)
        {
            for (int i = 0; i < vertices.size(); i++)
            {
                // Shift to origin
                //vertices[i].pos.x -= centerX;
                //vertices[i].pos.y -= centerY;
                //vertices[i].pos.z -= centerZ;

                // Uniform scale to make the bounding box fit [-1,1]
                //vertices[i].pos.x /= largestHalfExtent;
                //vertices[i].pos.y /= largestHalfExtent;
                //vertices[i].pos.z /= largestHalfExtent;
            }
        }
        */
        UnigmaRenderingStruct renderStruct = UnigmaRenderingStruct();
        renderStruct.vertices = vertices;
        renderStruct.indices = indices;

        renderCopy._renderer = renderStruct;


        QTDoughApplication::instance->AddRenderObject(&renderCopy, gObj, i);
    }

    std::cout << "Scene Loaded!" << std::endl;

    std::cout << "Model Information and Material Size: " << model.materials.size() << std::endl;
}