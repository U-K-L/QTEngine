#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <iostream>
#include <string>
#include <vector>
#include <../glm//glm.hpp>
#include "../GlobalObjects.h"



class Component
{
	public:
	Component();
	~Component();

	virtual void Update();
	virtual void Start();

	uint32_t GID; //ID of the gameObject this component is tied to.
	uint32_t CID; //ID of the component. For now this is set inside the code for each component, later we'll use a table.
	std::string Name;
	bool IsActive;
	bool IsCreated;
	bool IsStarted;
};
