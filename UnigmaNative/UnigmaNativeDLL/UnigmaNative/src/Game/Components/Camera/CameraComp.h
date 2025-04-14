#pragma once
//First example of component system so adding a ton of comments for this one.
//Let's include a general component header file.
#include "../Component.h"
#include "../../../Rendering/UnigmaCamera.h"

//And now the systems that every component has called.

class CameraComp : public Component
{
	public:
	CameraComp();
	~CameraComp();

	void Update() override;
	void Start() override;

	//This is the camera struct that will be used to render the scene.
	static constexpr const char* TypeName = "CameraComp";
	uint32_t CameraID;
	UnigmaCameraStruct* camera;
};
