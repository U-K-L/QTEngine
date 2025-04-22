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

	//Should only be dynamic.
	PxRigidDynamic *dynamicBody = static_cast<PxRigidDynamic*>(physicsComp->actor);

	// Quick print of physics.
	//Arrow keys move character
	dynamicBody->addForce(PxVec3(Controller0->movement.x, Controller0->movement.y, 0) * 10.0f, PxForceMode::eFORCE, true);
}

void PedestrianComp::Start()
{
}