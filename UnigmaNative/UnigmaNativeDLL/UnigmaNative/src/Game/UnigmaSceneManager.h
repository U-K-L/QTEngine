#pragma once
#include <iostream>
#include "UnigmaScene.h"

class UnigmaSceneManager
{
	public:
	static UnigmaSceneManager* instance;
	static void SetInstance(UnigmaSceneManager* app)
	{
		std::cout << "Setting Scene Manager instance" << std::endl;
		instance = app;
	}
	UnigmaSceneManager();
	~UnigmaSceneManager();

	void Update();
	void Start();
	void CreateScene(std::string sceneName);
	void AddScene(UnigmaScene scene);
	void LoadScene(std::string sceneName);
	void CleanUpAllScenes();

	uint32_t Index;
	std::vector<uint32_t> ScenesIndex;

	UnigmaScene* GetCurrentScene();
	void SetCurrentScene(UnigmaScene* scene);

	private:
		UnigmaScene* CurrentScene;
};
