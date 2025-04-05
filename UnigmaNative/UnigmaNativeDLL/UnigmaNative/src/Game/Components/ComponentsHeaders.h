#pragma once

//Add all the component headers.
#include "Component.h"
#include "Camera/CameraComp.h"
#include "UnigmaPhysicsComp/UnigmaPhysicsComp.h"

//Maps ints to the correct component.
Component* GetComponent(uint32_t componentType);