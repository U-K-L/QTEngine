#include "UnigmaSceneManager.h"
#include "GlobalObjects.h"
#include "UnigmaGameManager.h"

UnigmaSceneManager* UnigmaSceneManager::instance = nullptr; // Define the static member

UnigmaSceneManager::UnigmaSceneManager()
{

}

UnigmaSceneManager::~UnigmaSceneManager()
{
}

void UnigmaSceneManager::Update()
{
	//Updates all scenes.
	for(auto scene = GlobalScenes.begin(); scene != GlobalScenes.end(); scene++)
	{
		if(scene->second.IsActive && scene->second.IsCreated) //double check.
		{
			//Update scene.
			scene->second.Update();
		}
	}
}

//This actually starts the scene as if it were active.
void UnigmaSceneManager::Start()
{
	//Starts all scenes.
	for(auto scene = GlobalScenes.begin(); scene != GlobalScenes.end(); scene++)
	{
		if(scene->second.IsActive && scene->second.IsCreated && !scene->second.IsStarted)
		{
			//Start scene.
			scene->second.Start();
		}
	}

}

//Just creates a scene, but doesn't do anything with it.
void UnigmaSceneManager::CreateScene(std::string sceneName)
{
	UnigmaScene scene;
	scene.Name = sceneName;
	scene.IsActive = true;
	scene.CreateScene();

	std::cout << "Scene " << sceneName << " created" << std::endl;
	scene.IsCreated = true;
	AddScene(scene);

}

//Loads a scene from JSON file.
void UnigmaSceneManager::LoadScene(std::string sceneName)
{
	//Checks if already exists.
	if(GlobalScenes.count(sceneName) == 0)
	{
		std::cout << "Scene with name " << sceneName << " does not exist" << std::endl;
		return;
	}
	UnigmaScene scene = GlobalScenes[sceneName];

	//Loads scene via Json.
	scene.LoadJSON(sceneName.c_str());

	//Loads the renderer information for the scene.
	UnigmaGameManager::instance->LoadScene(sceneName.c_str());
}

void UnigmaSceneManager::AddScene(UnigmaScene scene)
{
	if(GlobalScenes.count(scene.Name) > 0)
	{
		std::cout << "Scene with name " << scene.Name << " already exists" << std::endl;
		return;
	}

	GlobalScenes.insert( std::pair<std::string, UnigmaScene>(scene.Name, scene));
	SetCurrentScene(&GlobalScenes[scene.Name]);
}

UnigmaScene* UnigmaSceneManager::GetCurrentScene()
{
	if(CurrentScene == nullptr)
	{
		std::cout << "Current scene is null" << std::endl;
		return nullptr;
	}
	return CurrentScene;
}

void UnigmaSceneManager::SetCurrentScene(UnigmaScene* scene)
{
		CurrentScene = scene;
}

void UnigmaSceneManager::CleanUpAllScenes()
{
	std::cout << "Cleaning up all scenes" << std::endl;
	for(auto scene = GlobalScenes.begin(); scene != GlobalScenes.end(); scene++)
	{
		scene->second.CleanUpScene();
	}
	GlobalScenes.clear();
}

