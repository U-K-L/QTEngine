#include "ComponentsHeaders.h"

std::unordered_map<std::string, int> ComponentMap = {
	{"CameraComp", 1},
	{"UnigmaPhysicsComp", 2},
	{"PedestrianComp", 3},
	{"RenderComp", 4}

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
	case 3: {
		//Pedestrian component
		PedestrianComp* pedestrian = new PedestrianComp();
		pedestrian->CID = 3;
		return pedestrian;
	}
	case 4: {
		//Render component
		RenderComp* render = new RenderComp();
		render->CID = 4;
		return render;
	}
	default: {
		return nullptr;
	}
	}
}