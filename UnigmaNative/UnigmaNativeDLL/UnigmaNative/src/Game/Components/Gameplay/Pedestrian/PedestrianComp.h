#pragma once
#include "../../Component.h"

class PedestrianComp : public Component
{
public:
	PedestrianComp();
	~PedestrianComp();

	void InitializeData(nlohmann::json& componentData) override;
	void Update() override;
	void Start() override;
};
