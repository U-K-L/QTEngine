#include "PedestrianComp.h"
//Add game manager
#include "../../../UnigmaGameManager.h"

//SDL
#include <SDL.h>
#include "../../../../Core/InputHander.h"

PedestrianComp::PedestrianComp()
{
}

PedestrianComp::~PedestrianComp()
{
}

void PedestrianComp::InitializeData(nlohmann::json& componentData)
{
}

void PedestrianComp::Update()
{
	UnigmaGameManager* gameManager = UnigmaGameManager::instance;
	UnigmaInputStruct* Controller0 = &UnigmaGameManager::instance->Controller0;
	UnigmaPhysicsComp* physicsComp = gameManager->GetObjectComponent<UnigmaPhysicsComp>(*gameObjectClass);

	// Quick print of physics.
	//std::cout << "Physics: " << physicsComp->geometryType << std::endl;
	//Arrow keys move character
	

}

void PedestrianComp::Start()
{
}