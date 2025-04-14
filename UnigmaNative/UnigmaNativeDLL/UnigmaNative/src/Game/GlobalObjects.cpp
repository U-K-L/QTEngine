#include "GlobalObjects.h"

UnigmaGameObject GameObjects[MAX_NUM_GAMEOBJECTS];
std::vector<UnigmaCameraStruct> Cameras;
std::vector<UnigmaLight> Lights;

std::unordered_map<std::string, UnigmaScene> GlobalScenes;
std::vector<UnigmaGameObjectClass> GameObjectsClasses;
//std::vector<UnigmaRenderingStruct> RenderingObjects;