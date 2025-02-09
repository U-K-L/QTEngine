#include "UnigmaRenderingManager.h"

UnigmaRenderingManager::UnigmaRenderingManager()
{

}

UnigmaRenderingManager::~UnigmaRenderingManager()
{
}

void UnigmaRenderingManager::Update()
{
}

void UnigmaRenderingManager::Start()
{
}

void UnigmaRenderingManager::CreateRenderingObject(UnigmaGameObject& gameObject)
{
	UnigmaRenderingStruct renderingObject = UnigmaRenderingStruct();
	renderingObject.GID = gameObject.ID;

	//print name and id and say where its from.
	std::cout << "Creating rendering object for: " << gameObject.name << " with ID: " << gameObject.ID << std::endl;

	GameObjects[gameObject.ID].RenderID = RenderingObjects.size();
	RenderingObjects.push_back(renderingObject);
}

void UnigmaRenderingManager::CreateLightObject(UnigmaGameObject& gameObject)
{
	UnigmaLight lightObject = UnigmaLight();

	lightObject.position = gameObject.transform.position;
	lightObject.direction = gameObject.transform.forward();


	Lights.push_back(lightObject);
}
