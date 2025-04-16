#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <iostream>
#include <string>
#include <vector>
#include <../glm//glm.hpp>
#include "../GlobalObjects.h"
#include "../json/json.hpp"
#include "../../Game/UnigmaSceneManager.h"



class Component
{
	public:
	Component();
	~Component();

	virtual void InitializeData(nlohmann::json& componentData);
	virtual void Update();
	virtual void Start();

	static constexpr const char* TypeName = "ComponentNameComp";
	uint32_t GID; //ID of the gameObject this component is tied to.
	uint32_t CID; //ID of the component. For now this is set inside the code for each component, later we'll use a table.
	uint32_t GlobalIndexID; //Global index ID for the component.
	std::string Name;
	bool IsActive;
	bool IsCreated;
	bool IsStarted;
};
