#include "Component.h"

Component::Component()
{
}

Component::~Component()
{
}

void Component::InitializeData(nlohmann::json& componentData)
{
	std::cout << "Initializing Component" << std::endl;
}

void Component::Update()
{
}

void Component::Start()
{
}