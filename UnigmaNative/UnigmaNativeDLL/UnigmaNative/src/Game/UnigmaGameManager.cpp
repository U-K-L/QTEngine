#pragma once
#include "UnigmaGameManager.h"
#include "GlobalObjects.h"
#include <iostream>
#include <chrono>


UnigmaGameManager* UnigmaGameManager::instance = nullptr; // Define the static member
//pointer to the callback function
extern LoadSceneCallbackType LoadSceneCallbackPointer = nullptr;



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

	std::cout << "UnigmaGameManager created" << std::endl;

	SceneManager->CreateScene("SM_Deccer_Cubes_Textured_Complex");
	SceneManager->LoadScene("SM_Deccer_Cubes_Textured_Complex");

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

	bool processInput = false;
	while (SDL_PollEvent(&inputEvent))
	{
		//print all events.
		SceneManager->Update();
		RenderingManager->Update();

		//Update each component.
		for (auto& component : Components)
		{
			component->Update();
		}
		processInput = true;
	}
	if (processInput)
	{
		SDL_Delay(16);
		processInput = false;
	}

	/*
	if(elaspedTime > 0)
	{
		deltaTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - lastTime).count();
		SceneManager->Update();
		RenderingManager->Update();

		//Update each component.
		for (auto& component : Components)
		{
			component->Update();
		}
		lastTime = std::chrono::high_resolution_clock::now();
	}

	SDL_Delay(16);
	*/
}

void UnigmaGameManager::SortComponents()
{
	//Sort components by their CID.
	std::sort(Components.begin(), Components.end(), [](const auto& a, const auto& b) {
		return a->CID < b->CID;
	});
}

void UnigmaGameManager::AddComponent(UnigmaGameObject gObj, Component component)
{
	//Create the component according to a table mapper.
	Component* comp = GetComponent(component.CID);

	//Add the component to components array.
	Components.push_back(comp);

	//Set the GID of the component.
	comp->GID = gObj.ID;

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

//Register loading scene callback
UNIGMANATIVE_API void RegisterLoadSceneCallback(LoadSceneCallbackType callback)
{
	LoadSceneCallbackPointer = callback;
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
