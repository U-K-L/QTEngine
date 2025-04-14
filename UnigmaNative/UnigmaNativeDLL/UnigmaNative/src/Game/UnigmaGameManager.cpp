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
	RenderingManager = new UnigmaRenderingManager();
	PhysicsInitialize();

	std::cout << "UnigmaGameManager created" << std::endl;

	std::string defaultSceneString = "TestCubeScene";

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

void UnigmaGameManager::AddComponent(UnigmaGameObject& gObj, Component component, std::string compName)
{
	//Create the component according to a table mapper.
	Component* comp = GetComponent(ComponentMap[compName]);

	
	//Set the GID of the component.
	comp->GID = gObj.ID;

	//Add the component to components array.
	Components.push_back(comp);

	//Set the GlobalIndexID of the component.
	Components[Components.size()-1]->GlobalIndexID = Components.size() - 1;

	uint16_t GIndex = UnigmaGameManager::instance->Components[UnigmaGameManager::instance->Components.size() - 1]->GlobalIndexID;
	gObj.components[GameObjectsClasses[gObj.ID].components.size()] = GIndex;
	GameObjectsClasses[gObj.ID].components.insert({ compName, gObj.components[GameObjectsClasses[gObj.ID].components.size()] });

	//Sort the components.
	SortComponents();
}

UNIGMANATIVE_API UnigmaGameObject* GetGameObject(uint32_t ID)
{
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
