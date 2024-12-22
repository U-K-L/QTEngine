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
	default: {
		return nullptr;
	}
	}
}