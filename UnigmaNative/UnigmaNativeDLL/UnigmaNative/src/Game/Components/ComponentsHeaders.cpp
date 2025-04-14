#include "ComponentsHeaders.h"

std::unordered_map<std::string, int> ComponentMap = {
	{"CameraComp", 1},
	{"UnigmaPhysicsComp", 2}
};

Component* GetComponent(uint32_t componentType)
{
	switch (componentType)
	{
	case 1: {
		//Camera component
		CameraComp* camera = new CameraComp();
		camera->CID = 1;
		return camera;
	}
	case 2: {
		//Physics component
		UnigmaPhysicsComp* physics = new UnigmaPhysicsComp();
		physics->CID = 2;
		return physics;
	}
	default: {
		return nullptr;
	}
	}
}