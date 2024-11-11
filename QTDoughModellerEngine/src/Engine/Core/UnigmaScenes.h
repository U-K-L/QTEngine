#pragma once
#include <stdio.h>
#include <vector>
#include "UnigmaGameObject.h"
#include "../RenderPasses/RenderPassObject.h"

class UnigmaScene
{
	public:
		std::vector<UnigmaGameObject> GameObjects;
		std::vector<RenderPassObject> RenderPasses;
};