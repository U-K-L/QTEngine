#include "ComponentsHeaders.h"

Component* GetComponent(uint32_t componentType)
{
	switch (componentType)
	{
	case 1: {
		//Camera component
		CameraComp* camera = new CameraComp();
		return camera;
	}
	case 2: {
		//Physics component
		UnigmaPhysicsComp* physics = new UnigmaPhysicsComp();
		return physics;
	}
	default: {
		return nullptr;
	}
	}
}