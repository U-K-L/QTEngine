#pragma once
#include "UnigmaGameManager.h"
#include "GlobalObjects.h"
#include <iostream>
#include <chrono>


UnigmaGameManager* UnigmaGameManager::instance = nullptr; // Define the static member
//pointer to the callback function
extern LoadSceneCallbackType LoadSceneCallbackPointer = nullptr;
extern LoadInputCallbackType LoadInputCallbackPointer = nullptr;



UnigmaGameManager::UnigmaGameManager()
{

}


UnigmaGameManager::~UnigmaGameManager()
{
}


void UnigmaGameManager::Create()
{
	//Create managers.
	SceneManager = new UnigmaSceneManager();
	SceneManager->SetInstance(SceneManager);
	RenderingManager = new UnigmaRenderingManager();
	PhysicsInitialize();

	std::cout << "UnigmaGameManager created" << std::endl;

	std::string defaultSceneString = "CubeExample";

	SceneManager->CreateScene(defaultSceneString);
	SceneManager->LoadScene(defaultSceneString);

	IsCreated = true;

	Start();
}


void UnigmaGameManager::LoadScene(const char* sceneName)
{
	// Print the scene being loaded
	std::cout << "Loading scene: " << sceneName << std::endl;

	// Start timing
	auto startTime = std::chrono::high_resolution_clock::now();

	// Load the scene using the callback
	LoadSceneCallbackPointer(sceneName);

	// End timing
	auto endTime = std::chrono::high_resolution_clock::now();

	// Calculate the duration in milliseconds
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

	// Print how long it took
	std::cout << "Scene loaded in " << duration << " milliseconds." << std::endl;
}

void UnigmaGameManager::Start()
{
	SceneManager->Start();

	//Start each component.
	for (auto& component : Components)
	{
		component->Start();
	}


}

void UnigmaGameManager::Update()
{
	auto elaspedTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - lastTime).count();
	deltaTime = elaspedTime;

	//SetButtonInputs();
	UnigmaInputStruct inputTemp = LoadInputCallbackPointer(0);
	if (inputTemp.inputReceived)
	{
		Controller0 = inputTemp;
	}

	SceneManager->Update();
	RenderingManager->Update();

	//Update each component.
	for (auto& component : Components)
	{
		component->Update();
	}

	lastTime = std::chrono::high_resolution_clock::now();

	// Get the time since the epoch in seconds as a floating-point value
	auto timeSinceEpoch = std::chrono::duration<float>(lastTime.time_since_epoch()).count();
	currentTime = timeSinceEpoch;
}

void UnigmaGameManager::SortComponents()
{
	//Sort components by their CID.
	std::sort(Components.begin(), Components.end(), [](const auto& a, const auto& b) {
		return a->CID < b->CID;
	});
}

void UnigmaGameManager::AddComponent(UnigmaGameObject& gObj, std::string compName)
{
	//Create the component according to a table mapper.
	Component* comp = GetComponent(ComponentMap[compName]);

	
	//Set the GID of the component.
	comp->GID = gObj.ID;

	comp->gameObjectClass = &GameObjectsClasses[gObj.ID];

	//Add the component to components array.
	Components.push_back(comp);

	//Set the GlobalIndexID of the component.
	Components[Components.size()-1]->GlobalIndexID = Components.size() - 1;

	uint16_t GIndex = UnigmaGameManager::instance->Components[UnigmaGameManager::instance->Components.size() - 1]->GlobalIndexID;

	gObj.components[GameObjectsClasses[gObj.ID].components.size()] = GIndex;
	GameObjectsClasses[gObj.ID].components.insert({ compName, GameObjectsClasses[gObj.ID].components.size() });

	//Sort the components.
	//SortComponents();
}

void UnigmaGameManager::EndGame()
{
	//Clean up all scenes.
	SceneManager->CleanUpAllScenes();
	PhysicsShutdown();
}

UnigmaGameObjectClass* GetGameObjectClass(uint32_t ID)
{
	auto gObj = &GameObjects[ID];
	auto gObjClass = &GameObjectsClasses[gObj->ID];
	return gObjClass;
	if (GameObjects[ID].isCreated)
	{
		return &GameObjectsClasses[gObj->ID];
	}
	else
	{
		return nullptr;
	}
}

Component* GetComponent(const UnigmaGameObjectClass& gObj, const char* componentName)
{
	std::string compNameStr = std::string(componentName);
	if (gObj.components.contains(compNameStr))
	{
		auto it = gObj.components.find(compNameStr);
		auto index = it->second;
		auto globalId = gObj.gameObject->components[index];

		Component* comp = UnigmaGameManager::instance->Components[globalId];

		return comp;
	}
	return nullptr;
}

UNIGMANATIVE_API UnigmaGameObject* GetGameObject(uint32_t ID)
{
	//REMOVE RETURN IN FUTURE....
	return &GameObjects[ID];
	if(GameObjects[ID].isCreated)
	{
		return &GameObjects[ID];
	}
	else
	{
		return nullptr;
	}
}


UNIGMANATIVE_API Value GetComponentAttribute(uint32_t GID, const char* componentName, const char* componentAttr)
{
	//Get Game Object so we can get the component attached.
	UnigmaGameObjectClass* gObj = GetGameObjectClass(GID);
	std::cout << "GID at Component attr: " << GID << std::endl;
	auto component = GetComponent(*gObj, componentName);
	Value val = component->GetAttribute(componentAttr);
	return val;
}


//Get cameras.
UNIGMANATIVE_API UnigmaCameraStruct* GetCamera(uint32_t ID)
{
	return &Cameras[ID];
}

//Size of Camera.
UNIGMANATIVE_API uint32_t GetCamerasSize()
{
	return Cameras.size();
}

//Get Light.
UNIGMANATIVE_API UnigmaLight* GetLight(uint32_t ID)
{
	if(Lights.size() == 0)
	{
		return nullptr;
	}
	if(ID >= Lights.size())
	{
		return nullptr;
	}
	return &Lights[ID];
}

//Size of Lights.
UNIGMANATIVE_API uint32_t GetLightsSize()
{
	return Lights.size();
}

//Register loading scene callback
UNIGMANATIVE_API void RegisterLoadSceneCallback(LoadSceneCallbackType callback)
{
	LoadSceneCallbackPointer = callback;
}

//Register loading input callback
UNIGMANATIVE_API void RegisterLoadInputCallback(LoadInputCallbackType callback)
{
	LoadInputCallbackPointer = callback;
}

// Function to get the size of the RenderingObjects vector
UNIGMANATIVE_API uint32_t GetRenderObjectsSize() {
    return UnigmaGameManager::instance->RenderingManager->RenderingObjects.size();
}

// Function to get an element from the RenderingObjects vector by index
UNIGMANATIVE_API UnigmaRenderingStruct* GetRenderObjectAt(uint32_t index) {
    auto& renderObjects = UnigmaGameManager::instance->RenderingManager->RenderingObjects;
    if (index < renderObjects.size()) {
        return &renderObjects[index];
    }
    else {
        return nullptr; // Return nullptr if index is out of bounds
    }
}
