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
	GameObjects[gameObject.ID].RenderID = RenderingObjects.size();
	RenderingObjects.push_back(renderingObject);
}
