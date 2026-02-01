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
	std::cout << "Value at get attribute call: " << v.data.f32 << std::endl;
	std::cout << "ID at call: " << GID << std::endl;
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