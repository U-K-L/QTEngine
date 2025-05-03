#pragma once
#include <iostream>
#include "UnigmaGameObject.h"
#include "../Rendering/UnigmaCamera.h"
#include <SDL.h>
#include "PxPhysicsAPI.h"
#include "PxScene.h"
#include "../json/json.hpp"


using namespace physx;
class UnigmaScene
{
	public:
	UnigmaScene();
	~UnigmaScene();

	void Update();
	void Start();
	void CreateScene();
	void AddGameObject(UnigmaGameObject& gameObject);
	void AddObjectComponents(UnigmaGameObject& gameObject, nlohmann::json& gameObjectData);
	void LoadJSON(std::string scenePath);
	void CleanUpScene();

	PxScene* physicsScene;
	uint32_t Index;
	std::vector<uint32_t> GameObjectsIndex;
	UnigmaGameObject camera;

	std::string Name;
	bool IsActive;
	bool IsCreated;
	bool IsStarted;
};