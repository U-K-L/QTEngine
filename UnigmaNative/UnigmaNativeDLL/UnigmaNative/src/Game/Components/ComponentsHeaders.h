#pragma once

//Add all the component headers.
#include "Component.h"
#include "Camera/CameraComp.h"
#include "UnigmaPhysicsComp/UnigmaPhysicsComp.h"
#include "Gameplay/Pedestrian/PedestrianComp.h"

//Maps ints to the correct component.
Component* GetComponent(uint32_t componentType);

extern std::unordered_map<std::string, int> ComponentMap;
