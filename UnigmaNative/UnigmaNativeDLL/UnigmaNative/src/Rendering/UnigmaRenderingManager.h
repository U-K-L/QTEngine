#pragma once
#include <iostream>
#include "../Game/UnigmaGameObject.h"
#include "../Game/GlobalObjects.h"
#include "UnigmaRenderingObject.h"

class UnigmaRenderingManager
{
	public:
	UnigmaRenderingManager();
	~UnigmaRenderingManager();

	void Update();
	void Start();

	std::vector<UnigmaRenderingStruct> RenderingObjects;
	void CreateRenderingObject(UnigmaGameObject& gameObject);
	void CreateLightObject(UnigmaGameObject& gameObject);
};