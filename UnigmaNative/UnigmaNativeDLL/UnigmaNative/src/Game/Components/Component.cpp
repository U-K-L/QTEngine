#include "Component.h"

Component::Component()
{
}

Component::~Component()
{
}

Value Component::GetAttribute(const char* componentAttribute)
{

	Value v = componentAttributes[std::string(componentAttribute)];
	return v;
}

void Component::InitializeData(nlohmann::json& componentData)
{

}

void Component::Update()
{
}

void Component::Start()
{
}