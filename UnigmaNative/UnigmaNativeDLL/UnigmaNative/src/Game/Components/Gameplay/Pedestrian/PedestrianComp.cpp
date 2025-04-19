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
	UnigmaInputStruct* Controller0 = &UnigmaGameManager::instance->Controller0;
	//Arrow keys move character
	

}

void PedestrianComp::Start()
{
}