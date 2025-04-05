#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "UnigmaScene.h"
#include "../glm/glm.hpp"
#include "GlobalObjects.h"
#include "UnigmaGameManager.h"
#include <fstream>
#include "../json/json.hpp"

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
	std::cout << "Delta Time: " << UnigmaGameManager::instance->deltaTime << std::endl;
	physicsScene->simulate(0.001f);
	physicsScene->fetchResults(true);

	UnigmaGameManager* gameManager = UnigmaGameManager::instance;
	UnigmaGameObject* gobj = &GameObjects[3];
	PxRigidDynamic* physicsCube = static_cast<PxRigidDynamic*>( static_cast<UnigmaPhysicsComp*>(gameManager->Components[1])->actor);
	if (physicsCube)
	{
		PxTransform pose = physicsCube->getGlobalPose();
		printf("Cube Position: x=%.2f, y=%.2f, z=%.2f\n",
			pose.p.x, pose.p.y, pose.p.z);
		printf("Cube Rotation: x=%.2f, y=%.2f, z=%.2f, w=%.2f\n",
			pose.q.x, pose.q.y, pose.q.z, pose.q.w);


		std::cout << "GameObject Name: " << gobj->name << std::endl;
		gobj->transform.position.x = pose.p.x;
		gobj->transform.position.y = pose.p.y;
		gobj->transform.position.z = pose.p.z;

		glm::quat q = glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z); // (w, x, y, z)
		glm::vec3 euler = glm::eulerAngles(q);
		gobj->transform.rotation.x = euler.x;
		gobj->transform.rotation.y = euler.y;
		gobj->transform.rotation.z = euler.z;

		gobj->transform.UpdateTransform();

	}
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
	for (const auto& obj : jsonData["GameObjects"]) {
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
	//gPhysicsFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gPhysicsAllocator, gPhysicsErrorCallback);
	//gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gPhysicsFoundation, PxTolerancesScale(), true);

	PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f);
	sceneDesc.cpuDispatcher = gPhysicsDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	std::cout << "Physics Scene desc created" << std::endl;

	physicsScene = gPhysics->createScene(sceneDesc);

	std::cout << "Physics Scene created" << std::endl;

	UnigmaGameManager* gameManager = UnigmaGameManager::instance;
	//LoadJSON("Assets/Scenes/WaveTestScene.json");

	//Create a camera.
	camera.SetName("mainCamera");

	//push on stack. Do this before adding components.
	AddGameObject(camera);

	//Create a camera component.
	Component cameraComponent;
	//CID determines which object, this will have a table mapping but for now manual.
	cameraComponent.CID = 1; //Camera component.
	gameManager->AddComponent(camera, cameraComponent);




	PxMaterial* material = gPhysics->createMaterial(0.5f, 0.5f, 0.6f); // static friction, dynamic friction, restitution

	std::cout << "Physics Material created" << std::endl;

	// Create a ground plane
	PxTransform groundPose(PxVec3(0, 0, -2.5));
	PxBoxGeometry groundBox(50, 50, 1); // Wide flat box at Z = 0
	PxRigidStatic* ground = gPhysics->createRigidStatic(groundPose);
	PxShape* shape = gPhysics->createShape(groundBox, *material);
	ground->attachShape(*shape);
	physicsScene->addActor(*ground);

	std::cout << "Ground plane created" << std::endl;


	// Create a dynamic cube
	UnigmaGameObject* gobj = &GameObjects[3];
	Component unigmaPhysicsComp;
	unigmaPhysicsComp.CID = 2; //Camera component.
	gameManager->AddComponent(*gobj, unigmaPhysicsComp);
	PxQuat rotation(PxPiDivTwo, PxVec3(1, 0, 0)); // rotate 90° around X
	PxTransform t(PxVec3(0, 0, 10), rotation);
	PxBoxGeometry boxGeom(1, 1, 1); // 2x2x2 cube

	std::cout << "Physics Transform created" << std::endl;



	UnigmaPhysicsComp* physicsComp = static_cast<UnigmaPhysicsComp*>(gameManager->Components[1]);

	physicsComp->geometryType = UnigmaPhysicsComp::Box;
	physicsComp->bodyType = UnigmaPhysicsComp::Dynamic;
	physicsComp->bounds = glm::vec3(1, 1, 1);
	physicsComp->transform = t;
	physicsComp->Init();

	physicsScene->addActor(*physicsComp->actor);




	std::cout << "Scene created" << std::endl;

}