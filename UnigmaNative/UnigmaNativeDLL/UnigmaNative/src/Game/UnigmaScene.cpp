#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "UnigmaScene.h"
#include "../glm/glm.hpp"
#include "GlobalObjects.h"
#include "UnigmaGameManager.h"
#include <fstream>

#include "../Physics/UnigmaPhysicsManager.h"


using json = nlohmann::json;

UnigmaScene::UnigmaScene()
{
}

UnigmaScene::~UnigmaScene()
{
}

void UnigmaScene::Update()
{
	float simulationTime = 1.0f / 24.0f;
	physicsScene->simulate(simulationTime);
	physicsScene->fetchResults(true);
}

void UnigmaScene::Start()
{

}

void UnigmaScene::AddGameObject(UnigmaGameObject& gameObject)
{
	//Add the game object to Game Manager global GameObjects array. And ensure proper indexing.
	std::cout << "Adding game object: " << gameObject.name << std::endl;
	std::cout << "Size of index: " << GameObjectsIndex.size() << std::endl;
	gameObject.ID = GameObjectsIndex.size();
	GameObjectsIndex.push_back(gameObject.ID); //Push this onto our scene gameobjects.
	GameObjects[gameObject.ID] = gameObject; //Globally overwrite with this.
	UnigmaGameObjectClass gameObjectClass;
	gameObjectClass.gameObject = &GameObjects[gameObject.ID];
	GameObjectsClasses.push_back(gameObjectClass);
}

void UnigmaScene::AddObjectComponents(UnigmaGameObject& gameObject, json& gameObjectData)
{
	UnigmaGameManager* gameManager = UnigmaGameManager::instance;
	UnigmaGameObjectClass* gameObjectClass = &GameObjectsClasses[gameObject.ID];
	UnigmaGameObject* gobj = gameObjectClass->gameObject;

	for (const auto& [componentName, componentData] : gameObjectData["Components"].items())
	{

		gameManager->AddComponent(*gobj, componentName);
		std::cout << "Adding component: " << componentName << std::endl;
		//Initialize the component data.
		if (GameObjectsClasses[gameObject.ID].components.contains(componentName)) {

			auto index = GameObjectsClasses[gameObject.ID].components[componentName];
			auto globalId = GameObjectsClasses[gameObject.ID].gameObject->components[index];

			Component* comp = gameManager->Components[globalId];
			std::cout << "Component initialized: " << componentName << std::endl;
			comp->InitializeData(componentData);

		}
	}

}

void UnigmaScene::LoadJSON(std::string sceneName)
{
	std::ifstream inputFile("Assets/Scenes/" + sceneName + "/" + sceneName + ".json");
	if (!inputFile.is_open()) {
		std::cerr << "Error: Unable to open file!" << std::endl;
	}

	json jsonData;
	inputFile >> jsonData;
	inputFile.close();

	//Get game manager.
	UnigmaGameManager* gameManager = UnigmaGameManager::instance;

	uint32_t jIndex = 0;
	// Parse GameObjects
	std::vector<UnigmaGameObject> gameObjects;
	for (auto& obj : jsonData["GameObjects"]) {
		UnigmaGameObject gameObject;


		//Get the name of this object.
		std::string name = obj["name"];
		strcpy(gameObject.name, name.c_str());

		//Get the positions.
		gameObject.transform.position = glm::vec3(obj["position"]["world"]["x"], obj["position"]["world"]["y"], obj["position"]["world"]["z"]);

		//Get the rotations.
		gameObject.transform.rotation = glm::vec3(obj["rotation"]["local"]["x"], obj["rotation"]["local"]["y"], obj["rotation"]["local"]["z"]);

		//Get the scales.
		gameObject.transform.scale = glm::vec3(obj["scale"]["world"]["x"], obj["scale"]["world"]["y"], obj["scale"]["world"]["z"]);

		gameObject.transform.UpdateTransform();

		gameObject.JID = jIndex;

		AddGameObject(gameObject);
		AddObjectComponents(gameObject, obj);

		jIndex++;



		if (strcmp(std::string(obj["type"]).c_str(), "Mesh") == 0)
		{
			std::cout << "Mesh object found: " << gameObject.name << std::endl;
			gameManager->RenderingManager->CreateRenderingObject(gameObject);
		}

		if(strcmp(std::string(obj["type"]).c_str(), "Light") == 0)
		{
			std::cout << "Light object found: " << gameObject.name << std::endl;
			gameManager->RenderingManager->CreateLightObject(gameObject);

			//Get size of lights.
			uint32_t size = GetLightsSize();
			UnigmaLight light;

			light.emission = glm::vec4(obj["Emission"]["r"], obj["Emission"]["g"], obj["Emission"]["b"], obj["Emission"]["a"]);

			Lights.push_back(light);

			std::cout << "Light Emission: " << Lights[size].emission.r << ", " << Lights[size].emission.g << ", " << Lights[size].emission.b << ", " << Lights[size].emission.a << std::endl;
		}
	}
}

void UnigmaScene::CreateScene()
{

	std::cout << "Creating scene" << std::endl;

	PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f);
	sceneDesc.cpuDispatcher = gPhysicsDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;


	physicsScene = gPhysics->createScene(sceneDesc);
	/*
	UnigmaGameManager* gameManager = UnigmaGameManager::instance;
	//Create a camera.
	camera.SetName("mainCamera");

	//push on stack. Do this before adding components.
	AddGameObject(camera);

	//Create a camera component.
	Component cameraComponent;
	//CID determines which object, this will have a table mapping but for now manual.
	cameraComponent.CID = 1; //Camera component.
	gameManager->AddComponent(camera, "CameraComp");
	*/
	std::cout << "Scene created" << std::endl;

}

void UnigmaScene::CleanUpScene()
{
	std::cout << "Cleaning up scene named: " << Name << std::endl;
	// in your cleanup, before gPhysicsDispatcher->release()�
	if (physicsScene)
	{
		physicsScene->release();
		physicsScene = nullptr;
	}

}