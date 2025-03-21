#pragma once
#include "UnigmaGameObject.h"
#include "UnigmaScene.h"
#include "unordered_map"
#include "../Rendering/UnigmaRenderingObject.h"
#include "../Rendering/UnigmaLight.h"


#define MAX_NUM_GAMEOBJECTS 8192
extern UnigmaGameObject GameObjects[MAX_NUM_GAMEOBJECTS];
extern std::vector<UnigmaCameraStruct> Cameras;
extern std::vector<UnigmaLight> Lights;
//extern std::vector<UnigmaRenderingStruct> RenderingObjects;

extern std::unordered_map<std::string, UnigmaScene> GlobalScenes;
