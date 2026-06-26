#include "Component.h"

Component::Component()
{
}

Component::~Component()
{
}

Value Component::GetAttribute(const char* componentAttribute)
{
	if (componentAttributes.contains(componentAttribute))
	{
		Value v = componentAttributes[std::string(componentAttribute)];
		return v;
	}
	else
		throw std::printf((std::string("ERROR GET ATTRIBUTE HAS No value found for: ") + std::string(componentAttribute)).c_str());

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